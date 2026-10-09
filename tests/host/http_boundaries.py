"""Compile the production HTTP accumulator/request functions against a fake client.
The fake deliberately ignores callback return codes, as clients may do.
"""
import pathlib, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[2]

def function(source, name):
    import re
    m = re.search(r"static (?:esp_err_t|bool) " + name + r"\(", source)
    assert m, name
    start = source.index("{", m.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[m.start():end]

COMMON = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define HTTP_EVENT_ON_DATA 1
#define HTTP_METHOD_POST 1
#define NETWORK_HA_URL_MAX 128
#define NETWORK_HA_TOKEN_MAX 256
#define ESP_LOGW(...) ((void)0)
typedef struct {int event_id, data_len; void *user_data; char *data;} esp_http_client_event_t;
typedef struct {const char *url; int timeout_ms; esp_err_t (*event_handler)(esp_http_client_event_t *); void *user_data; int buffer_size;} esp_http_client_config_t;
typedef const esp_http_client_config_t *esp_http_client_handle_t;
static size_t chunks[3];
static char bytes[4096];
static esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg) {return cfg;}
static void esp_http_client_set_header(esp_http_client_handle_t h,const char *key,const char *v) {(void)h;(void)key;(void)v;}
static void esp_http_client_set_method(esp_http_client_handle_t h,int m) {(void)h;(void)m;}
static void esp_http_client_set_post_field(esp_http_client_handle_t h,const char *p,size_t n) {(void)h;(void)p;(void)n;}
static int esp_http_client_get_status_code(esp_http_client_handle_t h) {(void)h;return 200;}
static void esp_http_client_cleanup(esp_http_client_handle_t h) {(void)h;}
static int esp_http_client_perform(esp_http_client_handle_t h) {
    for (int i=0;i<3;i++) {
        esp_http_client_event_t e={HTTP_EVENT_ON_DATA,(int)chunks[i],h->user_data,bytes};
        (void)h->event_handler(&e);
    }
    return ESP_OK;
}
'''

def run():
    with tempfile.TemporaryDirectory(prefix="robot-http-tests-") as td:
        td = pathlib.Path(td)
        for file, event, call, typ, field, used, config, capacity in [
            ("ha_devices.c", "on_event", "call", "reply_t", "body", "used", "config", 3072),
            ("ha_lights.c", "http_event", "request", "response_t", "data", "len", "s_config", 2048),
        ]:
            source = (ROOT / "components/network" / file).read_text()
            code = COMMON + f'''
typedef struct {{ char {field}[{capacity}]; size_t {used}; bool overflow; }} {typ};
static struct {{char ha_url[128],ha_token[256];}} {config}={{"http://test", "test"}};
''' + function(source, event) + "\n" + function(source, call) + f'''
int main(void) {{
    {typ} reply={{0}};
    memset(bytes,'x',sizeof(bytes));
    chunks[0]={capacity}-1;
    assert({call}("/state",NULL,&reply));
    assert(reply.{used}=={capacity}-1 && reply.{field}[{capacity}-1]==0);
    chunks[0]={capacity};
    assert(!{call}("/state",NULL,&reply));
    assert(reply.overflow);
    chunks[0]=5; chunks[1]={capacity}; chunks[2]=1;
    assert(!{call}("/state",NULL,&reply));
    assert(reply.overflow && reply.{used}==5);
    /* Reset on the next request; an empty data event is harmless. */
    chunks[0]=0; chunks[1]=3; chunks[2]=0;
    assert({call}("/state",NULL,&reply));
    assert(!reply.overflow && reply.{used}==3);
    chunks[0]={capacity}; chunks[1]=0;
    assert({call}("/service","{{}}",NULL));
    puts("PASS: {file} exact capacity, chunk overflow, latched failure, reset, ignored POST body");
}}
'''
            test = td / (file + ".test.c")
            test.write_text(code)
            exe = td / (file + ".test")
            subprocess.run(["cc", "-std=c11", "-fsanitize=address,undefined", str(test), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    run()
