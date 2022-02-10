#include "schemeshard__operation_part.h"
#include "schemeshard__operation_common.h"
#include "schemeshard_impl.h"

#include <ydb/core/base/subdomain.h>

namespace {

using namespace NKikimr;
using namespace NSchemeShard;

void DropPath(NIceDb::TNiceDb& db,
              TOperationContext& context,
              TOperationId operationId,
              const TTxState& txState,
              TPath& path)
{
    if (path->Dropped()) {
        // it might be dropped
        // when rolling update goes
        // old code drop the path early at TDropParts
        // and new code try do it once more time for sure
        return;
    }

    Y_VERIFY(txState.PlanStep);
    path->SetDropped(txState.PlanStep, operationId.GetTxId());
    context.SS->PersistDropStep(db, path->PathId, txState.PlanStep, operationId);

    context.SS->TabletCounters->Simple()[COUNTER_TABLE_COUNT].Sub(1);
    const i64 partitionsCount = txState.Shards.size();
    context.SS->TabletCounters->Simple()[COUNTER_TABLE_SHARD_ACTIVE_COUNT].Sub(partitionsCount);
    context.SS->TabletCounters->Simple()[COUNTER_TABLE_SHARD_INACTIVE_COUNT].Add(partitionsCount);

    context.SS->TabletCounters->Simple()[COUNTER_USER_ATTRIBUTES_COUNT].Sub(path->UserAttrs->Size());
    context.SS->PersistUserAttributes(db, path->PathId, path->UserAttrs, nullptr);

    auto domainInfo = context.SS->ResolveDomainInfo(path->PathId);
    domainInfo->DecPathsInside();

    auto parentDir = path.Parent();
    parentDir->DecAliveChildren();
    ++parentDir->DirAlterVersion;
    context.SS->PersistPathDirAlterVersion(db, parentDir.Base());

    context.SS->ClearDescribePathCaches(parentDir.Base());
    context.OnComplete.PublishToSchemeBoard(operationId, parentDir->PathId);

    context.SS->ClearDescribePathCaches(path.Base());
    context.OnComplete.PublishToSchemeBoard(operationId, path->PathId);
}

class TDropParts: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override { 
        return TStringBuilder()
                << "TDropTable TDropParts"
                << " operationId#" << OperationId;
    }

public:
    TDropParts(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {});
    }

    bool HandleReply(TEvDataShard::TEvProposeTransactionResult::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvProposeTransactionResult"
                               << " at tabletId# " << ssId);
        LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    DebugHint() << " HandleReply TEvProposeTransactionResult"
                                << " message# " << ev->Get()->Record.ShortDebugString());

        if (!NTableState::CollectProposeTransactionResults(OperationId, ev, context)) {
            return false;
        }

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxDropTable);
        Y_VERIFY(txState->MinStep);

        return true;
    }


    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxDropTable);

        if (NTableState::CheckPartitioningChangedForTableModification(*txState, context)) {
            LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                        DebugHint() << " UpdatePartitioningForTableModification");
            NTableState::UpdatePartitioningForTableModification(OperationId, *txState, context);
        }

        txState->ClearShardsInProgress();

        TString txBody;
        {
            TPathId pathId = txState->TargetPathId;
            Y_VERIFY(context.SS->PathsById.contains(pathId));
            TPathElement::TPtr path = context.SS->PathsById.at(pathId);
            Y_VERIFY(path);

            auto seqNo = context.SS->StartRound(*txState);

            NKikimrTxDataShard::TFlatSchemeTransaction tx;
            context.SS->FillSeqNo(tx, seqNo);
            tx.MutableDropTable()->SetId_Deprecated(pathId.LocalPathId);
            PathIdFromPathId(pathId, tx.MutableDropTable()->MutablePathId());
            tx.MutableDropTable()->SetName(path->Name);
            Y_PROTOBUF_SUPPRESS_NODISCARD tx.SerializeToString(&txBody);
        }

        Y_VERIFY(txState->Shards.size());
        for (ui32 i = 0; i < txState->Shards.size(); ++i) {
            auto idx = txState->Shards[i].Idx;
            auto datashardId = context.SS->ShardInfos[idx].TabletID;

            THolder<TEvDataShard::TEvProposeTransaction> event =
                MakeHolder<TEvDataShard::TEvProposeTransaction>(NKikimrTxDataShard::TX_KIND_SCHEME,
                                                        context.SS->TabletID(),
                                                        context.Ctx.SelfID,
                                                        ui64(OperationId.GetTxId()),
                                                        txBody,
                                                        context.SS->SelectProcessingPrarams(txState->TargetPathId));

            context.OnComplete.BindMsgToPipe(OperationId, datashardId, idx, event.Release());
        }

        txState->UpdateShardsInProgress(TTxState::DropParts);
        return false;
    }
};

class TPropose: public TSubOperationState {
private:
    TOperationId OperationId;
    TTxState::ETxState& NextState;

    TString DebugHint() const override { 
        return TStringBuilder()
                << "TDropTable TPropose"
                << " operationId#" << OperationId;
    }

public:
    TPropose(TOperationId id, TTxState::ETxState& nextState)
        : OperationId(id)
        , NextState(nextState)
    {
        IgnoreMessages(DebugHint(), {TEvDataShard::TEvProposeTransactionResult::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvDataShard::TEvSchemaChanged"
                               << " triggers early, save it"
                               << ", at schemeshard: " << ssId);

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvOperationPlan::TPtr& ev, TOperationContext& context) override {
        TStepId step = TStepId(ev->Get()->StepId);
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvOperationPlan"
                               << ", step: " << step
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState->TxType == TTxState::TxDropTable);

        TPath path = TPath::Init(txState->TargetPathId, context.SS);
        Y_VERIFY(path.IsResolved());

        NIceDb::TNiceDb db(context.Txc.DB);

        txState->PlanStep = step;
        context.SS->PersistTxPlanStep(db, OperationId, step);

        if (context.SS->EnableSchemeTransactionsAtSchemeShard) {

            // only persist step, but do not set drop plan step for the path
            NextState = TTxState::WaitShadowPathPublication;
            context.SS->ChangeTxState(db, OperationId, TTxState::WaitShadowPathPublication);
            return true;
        }

        DropPath(db, context, OperationId, *txState, path);

        NextState = TTxState::ProposedWaitParts;
        context.SS->ChangeTxState(db, OperationId, TTxState::ProposedWaitParts);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxDropTable);

        TSet<TTabletId> shardSet;
        for (const auto& shard : txState->Shards) {
            TShardIdx idx = shard.Idx;
            Y_VERIFY(context.SS->ShardInfos.contains(idx));
            TTabletId tablet = context.SS->ShardInfos.at(idx).TabletID;
            shardSet.insert(tablet);
        }

        context.OnComplete.ProposeToCoordinator(OperationId, txState->TargetPathId, txState->MinStep, shardSet);
        return false;
    }
};

class TWaitRenamedPathPublication: public TSubOperationState {
private:
    TOperationId OperationId;

    TPathId ActivePathId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TDropTable TWaitRenamedPathPublication"
                << " operationId: " << OperationId;
    }

public:
    TWaitRenamedPathPublication(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvHive::TEvCreateTabletReply::EventType, TEvDataShard::TEvProposeTransactionResult::EventType, TEvPrivate::TEvOperationPlan::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvDataShard::TEvSchemaChanged"
                               << ", save it"
                               << ", at schemeshard: " << ssId);

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvCompletePublication::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvPrivate::TEvCompletePublication"
                               << ", msg: " << ev->Get()->ToString()
                               << ", at tablet" << ssId);

        Y_VERIFY(ActivePathId == ev->Get()->PathId);

        NIceDb::TNiceDb db(context.Txc.DB);
        context.SS->ChangeTxState(db, OperationId, TTxState::DeletePathBarrier);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        context.OnComplete.RouteByTabletsFromOperation(OperationId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", operation type: " << TTxState::TypeName(txState->TxType)
                               << ", at tablet" << ssId);


        TPath path = TPath::Init(txState->TargetPathId, context.SS);
        if (path.IsActive()) {
            LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                       DebugHint() << " ProgressState"
                                   << ", no renaming has been detected for this operation");

            NIceDb::TNiceDb db(context.Txc.DB);
            context.SS->ChangeTxState(db, OperationId, TTxState::DeletePathBarrier);
            return true;
        }

        auto activePath = TPath::Resolve(path.PathString(), context.SS);
        Y_VERIFY(activePath.IsResolved());

        Y_VERIFY(activePath != path);

        ActivePathId = activePath->PathId;
        context.OnComplete.PublishAndWaitPublication(OperationId, activePath->PathId);

        return false;
    }
};


class TDeleteTableBarrier: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TDropTable TDeleteTableBarrier"
                << " operationId: " << OperationId;
    }

public:
    TDeleteTableBarrier(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvHive::TEvCreateTabletReply::EventType, TEvDataShard::TEvProposeTransactionResult::EventType, TEvPrivate::TEvOperationPlan::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvDataShard::TEvSchemaChanged"
                               << ", save it"
                               << ", at schemeshard: " << ssId);

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvCompleteBarrier::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvPrivate::TEvCompleteBarrier"
                               << ", msg: " << ev->Get()->ToString()
                               << ", at tablet" << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        NIceDb::TNiceDb db(context.Txc.DB);

        TPath path = TPath::Init(txState->TargetPathId, context.SS);
        DropPath(db, context, OperationId, *txState, path);

        context.SS->ChangeTxState(db, OperationId, TTxState::ProposedWaitParts);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        context.OnComplete.RouteByTabletsFromOperation(OperationId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", operation type: " << TTxState::TypeName(txState->TxType)
                               << ", at tablet" << ssId);

        context.OnComplete.Barrier(OperationId, "RenamePathBarrier");

        return false;
    }
};


class TProposedDeletePart: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override { 
        return TStringBuilder()
                << "TDropTable TProposedDeletePart"
                << " operationId: " << OperationId;
    }

public:
    TProposedDeletePart(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvDataShard::TEvProposeTransactionResult::EventType, TEvPrivate::TEvOperationPlan::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        const TActorId& ackTo = ev->Sender;

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TProposedDeletePart"
                               << " repeated message, ack it anyway"
                               << " at tablet: " << ssId);

        THolder<TEvDataShard::TEvSchemaChangedResult> event = MakeHolder<TEvDataShard::TEvSchemaChangedResult>();
        event->Record.SetTxId(ui64(OperationId.GetTxId()));

        context.OnComplete.Send(ackTo, std::move(event));
        return false;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxDropTable);

        NIceDb::TNiceDb db(context.Txc.DB);

        context.SS->PersistRemoveTable(db, txState->TargetPathId, context.Ctx);

        context.OnComplete.DoneOperation(OperationId);
        return true;
    }

};

class TDropTable: public TSubOperation {
private:
    const TOperationId OperationId;
    TTxState::ETxState State = TTxState::Invalid;
    TTxState::ETxState AfterPropose = TTxState::Invalid;

    const NKikimrSchemeOp::TModifyScheme Transaction;

    TTxState::ETxState NextState() {
        return TTxState::DropParts;
    }

    TTxState::ETxState NextState(TTxState::ETxState state) {
        switch(state) {
        case TTxState::Waiting:
        case TTxState::DropParts:
            return TTxState::Propose;
        case TTxState::Propose:
            return AfterPropose;

        case TTxState::WaitShadowPathPublication:
            return TTxState::DeletePathBarrier;
        case TTxState::DeletePathBarrier:
            return TTxState::ProposedWaitParts;

        case TTxState::ProposedWaitParts:
            return TTxState::ProposedDeleteParts;
        default:
            return TTxState::Invalid;
        }
        return TTxState::Invalid;
    }

    TSubOperationState::TPtr SelectStateFunc(TTxState::ETxState state) {
        switch(state) {
        case TTxState::Waiting:
        case TTxState::DropParts:
            return MakeHolder<TDropParts>(OperationId);
        case TTxState::Propose:
            return MakeHolder<TPropose>(OperationId, AfterPropose);
        case TTxState::WaitShadowPathPublication:
            return MakeHolder<TWaitRenamedPathPublication>(OperationId);
        case TTxState::DeletePathBarrier:
            return MakeHolder<TDeleteTableBarrier>(OperationId);
        case TTxState::ProposedWaitParts:
            return MakeHolder<NTableState::TProposedWaitParts>(OperationId);
        case TTxState::Done:
        case TTxState::ProposedDeleteParts:
            return MakeHolder<TProposedDeletePart>(OperationId);
        default:
            return nullptr;
        }
    }

    void StateDone(TOperationContext& context) override {
        State = NextState(State);

        if (State != TTxState::Invalid) {
            SetState(SelectStateFunc(State));
            context.OnComplete.ActivateTx(OperationId);
        }
    }

public:
    TDropTable(TOperationId id, const TTxTransaction& tx)
        : OperationId(id)
        , Transaction(tx)
    {}

    TDropTable(TOperationId id, TTxState::ETxState state)
        : OperationId(id)
        , State(state)
    {
        SetState(SelectStateFunc(state));
    }

    THolder<TProposeResponse> Propose(const TString&, TOperationContext& context) override {
        const TTabletId ssId = context.SS->SelfTabletId();

        const auto& drop = Transaction.GetDrop();

        const TString& parentPathStr = Transaction.GetWorkingDir();
        const TString& name = drop.GetName();

        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TDropTable Propose"
                         << ", path: " << parentPathStr << "/" << name
                         << ", pathId: " << drop.GetId()
                         << ", opId: " << OperationId
                         << ", at schemeshard: " << ssId);

        auto result = MakeHolder<TProposeResponse>(NKikimrScheme::StatusAccepted, ui64(OperationId.GetTxId()), ui64(ssId));

        TPath path = drop.HasId()
            ? TPath::Init(context.SS->MakeLocalId(drop.GetId()), context.SS)
            : TPath::Resolve(parentPathStr, context.SS).Dive(name);

        {
            TPath::TChecker checks = path.Check();
            checks
                .NotEmpty()
                .NotUnderDomainUpgrade()
                .IsAtLocalSchemeShard()
                .IsResolved()
                .NotDeleted()
                .IsTable()
                .NotUnderDeleting()
                .NotUnderOperation();

            if (!checks) {
                TString explain = TStringBuilder() << "path fail checks"
                                                   << ", path: " << path.PathString();
                auto status = checks.GetStatus(&explain);
                result->SetError(status, explain);
                if (path.IsResolved() && path.Base()->IsTable() && (path.Base()->PlannedToDrop() || path.Base()->Dropped())) {
                    result->SetPathDropTxId(ui64(path.Base()->DropTxId));
                    result->SetPathId(path.Base()->PathId.LocalPathId);
                }
                return result;
            }
        }

        TPath parent = path.Parent();
        {
            TPath::TChecker checks = parent.Check();
            checks
                .NotEmpty()
                .IsResolved()
                .NotDeleted();

            if (checks) {
                if (parent.Base()->IsTableIndex()) {
                    checks
                        .IsTableIndex()
                        .IsInsideTableIndexPath()
                        .IsUnderDeleting()
                        .IsUnderTheSameOperation(OperationId.GetTxId()); //allow only as part of drop base table
                } else {
                    checks
                        .IsLikeDirectory()
                        .IsCommonSensePath()
                        .NotUnderDeleting();
                }
            }

            if (!checks) {
                TString explain = TStringBuilder() << "parent path fail checks"
                                                   << ", path: " << parent.PathString();
                auto status = checks.GetStatus(&explain);
                result->SetError(status, explain);
                return result;
            }
        }

        TString errStr;
        if (!context.SS->CheckApplyIf(Transaction, errStr)) {
            result->SetError(NKikimrScheme::StatusPreconditionFailed, errStr);
            return result;
        }

        if (!context.SS->CheckLocks(path.Base()->PathId, Transaction, errStr)) {
            result->SetError(NKikimrScheme::StatusMultipleModifications, errStr);
            return result;
        }

        context.MemChanges.GrabNewTxState(context.SS, OperationId);
        context.MemChanges.GrabPath(context.SS, path.Base()->PathId);
        context.MemChanges.GrabPath(context.SS, parent.Base()->PathId);

        context.DbChanges.PersistTxState(OperationId);
        context.DbChanges.PersistPath(path.Base()->PathId);
        context.DbChanges.PersistPath(parent.Base()->PathId);

        TTxState& txState = context.SS->CreateTx(OperationId, TTxState::TxDropTable, path.Base()->PathId);
        txState.MinStep = TStepId(1);
        txState.State = TTxState::DropParts;

        // Dirty hack: drop step must not be zero because 0 is treated as "hasn't been dropped"

        Y_VERIFY(context.SS->Tables.contains(path.Base()->PathId));
        TTableInfo::TPtr table = context.SS->Tables.at(path.Base()->PathId);
        Y_VERIFY(table->GetPartitions().size());
        for (auto& shard : table->GetPartitions()) {
            auto shardIdx = shard.ShardIdx;
            context.MemChanges.GrabShard(context.SS, shardIdx);
            context.DbChanges.PersistShard(shardIdx);

            Y_VERIFY_S(context.SS->ShardInfos.contains(shardIdx), "Unknown shardIdx " << shardIdx);
            txState.Shards.emplace_back(shardIdx, context.SS->ShardInfos[shardIdx].TabletType, TTxState::DropParts);

            context.SS->ShardInfos[shardIdx].CurrentTxId = OperationId.GetTxId();
        }

        path.Base()->PathState = TPathElement::EPathState::EPathStateDrop;
        path.Base()->DropTxId = OperationId.GetTxId();
        path.Base()->LastTxId = OperationId.GetTxId();

        IncParentDirAlterVersionWithRepublish(OperationId, path, context);

        for (auto splitTx: table->GetSplitOpsInFlight()) {
            context.OnComplete.Dependence(splitTx.GetTxId(), OperationId.GetTxId());
        }

        State = NextState();
        SetState(SelectStateFunc(State));
        context.OnComplete.ActivateTx(OperationId);
        return result;
    }

    void AbortPropose(TOperationContext& context) override {
        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TDropTable AbortPropose"
                         << ", opId: " << OperationId
                         << ", at schemeshard: " << context.SS->TabletID());
    }

    void AbortUnsafe(TTxId forceDropTxId, TOperationContext& context) override {
        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TDropTable AbortUnsafe"
                         << ", opId: " << OperationId
                         << ", forceDropId: " << forceDropTxId
                         << ", at schemeshard: " << context.SS->TabletID());

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        TPathId pathId = txState->TargetPathId;
        Y_VERIFY(context.SS->PathsById.contains(pathId));
        TPathElement::TPtr path = context.SS->PathsById.at(pathId);
        Y_VERIFY(path);

        if (path->Dropped()) {
            for (auto shard : txState->Shards) {
                context.OnComplete.DeleteShard(shard.Idx);
            }
        }

        context.OnComplete.DoneOperation(OperationId);
    }
};

}

namespace NKikimr {
namespace NSchemeShard {

ISubOperationBase::TPtr CreateDropTable(TOperationId id, const TTxTransaction& tx) {
    return new TDropTable(id, tx);
}

ISubOperationBase::TPtr CreateDropTable(TOperationId id, TTxState::ETxState state) {
    Y_VERIFY(state != TTxState::Invalid);
    return new TDropTable(id, state);
}

}
}
