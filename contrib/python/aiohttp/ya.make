# Generated by devtools/yamaker (pypi).

PY3_LIBRARY()

VERSION(3.9.5)

LICENSE(Apache-2.0)

PEERDIR(
    contrib/python/aiosignal
    contrib/python/attrs
    contrib/python/frozenlist
    contrib/python/multidict
    contrib/python/yarl
    contrib/restricted/llhttp
)

ADDINCL(
    contrib/restricted/llhttp/include
    contrib/python/aiohttp/aiohttp
    FOR cython contrib/python/aiohttp
)

NO_COMPILER_WARNINGS()

NO_LINT()

NO_CHECK_IMPORTS(
    aiohttp.pytest_plugin
    aiohttp.worker
)

SRCS(
    aiohttp/_find_header.c
)

PY_SRCS(
    TOP_LEVEL
    aiohttp/__init__.py
    aiohttp/_helpers.pyi
    aiohttp/abc.py
    aiohttp/base_protocol.py
    aiohttp/client.py
    aiohttp/client_exceptions.py
    aiohttp/client_proto.py
    aiohttp/client_reqrep.py
    aiohttp/client_ws.py
    aiohttp/compression_utils.py
    aiohttp/connector.py
    aiohttp/cookiejar.py
    aiohttp/formdata.py
    aiohttp/hdrs.py
    aiohttp/helpers.py
    aiohttp/http.py
    aiohttp/http_exceptions.py
    aiohttp/http_parser.py
    aiohttp/http_websocket.py
    aiohttp/http_writer.py
    aiohttp/locks.py
    aiohttp/log.py
    aiohttp/multipart.py
    aiohttp/payload.py
    aiohttp/payload_streamer.py
    aiohttp/pytest_plugin.py
    aiohttp/resolver.py
    aiohttp/streams.py
    aiohttp/tcp_helpers.py
    aiohttp/test_utils.py
    aiohttp/tracing.py
    aiohttp/typedefs.py
    aiohttp/web.py
    aiohttp/web_app.py
    aiohttp/web_exceptions.py
    aiohttp/web_fileresponse.py
    aiohttp/web_log.py
    aiohttp/web_middlewares.py
    aiohttp/web_protocol.py
    aiohttp/web_request.py
    aiohttp/web_response.py
    aiohttp/web_routedef.py
    aiohttp/web_runner.py
    aiohttp/web_server.py
    aiohttp/web_urldispatcher.py
    aiohttp/web_ws.py
    aiohttp/worker.py
    CYTHON_C
    aiohttp/_helpers.pyx
    aiohttp/_http_parser.pyx
    aiohttp/_http_writer.pyx
    aiohttp/_websocket.pyx
)

RESOURCE_FILES(
    PREFIX contrib/python/aiohttp/
    .dist-info/METADATA
    .dist-info/top_level.txt
    aiohttp/py.typed
)

END()
