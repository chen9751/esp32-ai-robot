#include "ha_devices.h"
#include "network_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *TAG="ha_devices";
typedef struct { char domain[16],service[32],entity[100],params[128]; } cmd_t;
static QueueHandle_t q;
static SemaphoreHandle_t lock;
static network_backend_config_t config;
static ha_devices_state_t state;
#define AC "climate.xiaomi_mt8_b9c8_air_conditioner"
#define BATH "climate.yeelink_v20_6acb_ptc_bath_heater"
#define CURTAIN "cover.xiaomi_acn010_a9ab_curtain"
#define RACK "cover.xiaomi_0002_bfe9_airer"
static const char *special[4]={"switch.xiaomi_mt8_b9c8_sleep_mode","switch.xiaomi_mt8_b9c8_eco","switch.xiaomi_mt8_b9c8_dryer","switch.xiaomi_mt8_b9c8_heater"};
static const char *bath_switch[3]={"switch.yeelink_v20_6acb_ventilation","switch.yeelink_v20_6acb_blow","switch.yeelink_v20_6acb_heating"};
static const char *bath_fan[2]={"select.yeelink_v20_6acb_fan_level_2","select.yeelink_v20_6acb_fan_level"};
static int jint(const cJSON *obj,const char *k,int fallback) {const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,k);return cJSON_IsNumber(v)?(int)(v->valuedouble+0.01):fallback;}
static const char *jstr(const cJSON *obj,const char *k) {const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool jbool(const cJSON *obj,const char *k) {const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,k);return cJSON_IsTrue(v);}
bool ha_devices_get(ha_devices_state_t *out) {if(!out||!lock||xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE)return false;*out=state;xSemaphoreGive(lock);return out->valid;}
bool ha_devices_command(const char *domain,const char *service,const char *entity,const char *params) {
 if(!q||!domain||!service||!entity)return false;
 cmd_t c={0};snprintf(c.domain,sizeof(c.domain),"%s",domain);snprintf(c.service,sizeof(c.service),"%s",service);snprintf(c.entity,sizeof(c.entity),"%s",entity);
 if(params)snprintf(c.params,sizeof(c.params),"%s",params);
 return xQueueSend(q,&c,0)==pdTRUE;
}
typedef struct{char body[3072];size_t used;}reply_t;
static esp_err_t on_event(esp_http_client_event_t *evt) {
 if(evt->event_id==HTTP_EVENT_ON_DATA&&evt->user_data&&evt->data_len>0) {
  reply_t *r=evt->user_data;size_t n=(size_t)evt->data_len;
  if(r->used+n>=sizeof(r->body))return ESP_FAIL;
  memcpy(r->body+r->used,evt->data,n);r->used+=n;r->body[r->used]=0;
 }return ESP_OK;
}
static bool call(const char *path,const char *body,reply_t *r) {
 char url[NETWORK_HA_URL_MAX+160];size_t n=strlen(config.ha_url);while(n&&config.ha_url[n-1]=='/')n--;
 if(n+strlen(path)+1>=sizeof(url))return false;
 snprintf(url,sizeof(url),"%.*s%s",(int)n,config.ha_url,path);
 esp_http_client_config_t cfg={.url=url,.timeout_ms=3500,.event_handler=on_event,.user_data=r,.buffer_size=1024};
 esp_http_client_handle_t h=esp_http_client_init(&cfg);if(!h)return false;
 char auth[NETWORK_HA_TOKEN_MAX+8];snprintf(auth,sizeof(auth),"Bearer %s",config.ha_token);
 esp_http_client_set_header(h,"Authorization",auth);
 if(body){esp_http_client_set_method(h,HTTP_METHOD_POST);esp_http_client_set_header(h,"Content-Type","application/json");esp_http_client_set_post_field(h,body,strlen(body));}
 if(r)memset(r,0,sizeof(*r));
 esp_err_t err=esp_http_client_perform(h);int status=err==ESP_OK?esp_http_client_get_status_code(h):0;
 esp_http_client_cleanup(h);
 if(err!=ESP_OK||status<200||status>=300){ESP_LOGW(TAG,"HA %s http=%d err=%s",path,status,esp_err_to_name(err));return false;}return true;
}
static cJSON *fetch(const char *entity) {
 char path[145];snprintf(path,sizeof(path),"/api/states/%s",entity);
 reply_t *r=calloc(1,sizeof(*r));if(!r)return NULL;
 cJSON *root=call(path,NULL,r)?cJSON_Parse(r->body):NULL;free(r);return root;
}
static void update_one(ha_devices_state_t *s,const char *id) {
 cJSON *r=fetch(id);if(!r)return;
 const char *v=jstr(r,"state");const cJSON *a=cJSON_GetObjectItemCaseSensitive(r,"attributes");
 if(!v){cJSON_Delete(r);return;}
 if(!strcmp(id,AC)){
   s->ac_on=strcmp(v,"off")!=0&&strcmp(v,"unavailable")!=0;
   const char *modes[]={"cool","heat","fan_only","dry"};
   for(int j=0;j<4;j++)if(!strcmp(v,modes[j]))s->ac_mode=j;
   const cJSON *t=cJSON_GetObjectItemCaseSensitive(a,"temperature");
   if(cJSON_IsNumber(t))s->ac_temp_x2=(int)(t->valuedouble*2+0.5);
   const char *fm=jstr(a,"fan_mode");s->ac_auto=fm&&!strcmp(fm,"auto");
   if(fm&&!strncmp(fm,"level",5))s->ac_fan=atoi(fm+5);
   s->ac_swing=jstr(a,"swing_mode")&&!strcmp(jstr(a,"swing_mode"),"on");
 }else if(!strcmp(id,CURTAIN))s->curtain_pos=jint(a,"current_position",s->curtain_pos);
 else if(!strcmp(id,RACK))s->rack_pos=100-jint(a,"current_position",100-s->rack_pos);
 else if(!strcmp(id,BATH)){
   const cJSON *t=cJSON_GetObjectItemCaseSensitive(a,"current_temperature");
   if(cJSON_IsNumber(t))s->bath_current_x10=(int)(t->valuedouble*10+0.5);
   t=cJSON_GetObjectItemCaseSensitive(a,"temperature");
   if(cJSON_IsNumber(t))s->bath_target_x10=(int)(t->valuedouble*10+0.5);
   const char *pm=jstr(a,"preset_mode");
   if(pm&&!strcmp(pm,"Dry"))s->bath_dry=true;
 }
 else {
  for(int i=0;i<4;i++)if(!strcmp(id,special[i]))s->ac_features[i]=!strcmp(v,"on");
  for(int i=0;i<3;i++)if(!strcmp(id,bath_switch[i]))s->bath_levels[i]=!strcmp(v,"on")?(s->bath_levels[i]?s->bath_levels[i]:1):0;
  if(!strcmp(id,"select.yeelink_v20_6acb_mode_2"))s->bath_dry=!strcmp(v,"Dry");
  for(int i=0;i<2;i++)if(!strcmp(id,bath_fan[i])){int k=i; /* vent=0, blow=1 */if(s->bath_levels[k])s->bath_levels[k]=!strcmp(v,"High")?2:1;}
 }
 cJSON_Delete(r);
}
static void refresh(void) {
 ha_devices_state_t next={0};
 if(lock&&xSemaphoreTake(lock,pdMS_TO_TICKS(50))==pdTRUE){next=state;xSemaphoreGive(lock);}
 update_one(&next,AC);update_one(&next,CURTAIN);update_one(&next,RACK);update_one(&next,BATH);
 for(int i=0;i<4;i++)update_one(&next,special[i]);
 for(int i=0;i<3;i++)update_one(&next,bath_switch[i]);
 update_one(&next,"select.yeelink_v20_6acb_mode_2");
 for(int i=0;i<2;i++)update_one(&next,bath_fan[i]);
 next.valid=true;
 if(xSemaphoreTake(lock,pdMS_TO_TICKS(50))==pdTRUE){state=next;xSemaphoreGive(lock);}
}
static void send_cmd(const cmd_t *c){
 cJSON *obj=cJSON_CreateObject();if(!obj)return;cJSON_AddStringToObject(obj,"entity_id",c->entity);
 if(c->params[0]){
  cJSON *params=cJSON_Parse(c->params);
  if(params&&cJSON_IsObject(params)){
   for(cJSON *p=params->child;p;){cJSON *next=p->next;cJSON_DetachItemViaPointer(params,p);cJSON_AddItemToObject(obj,p->string,p);p=next;}
  }cJSON_Delete(params);
 }
 char *body=cJSON_PrintUnformatted(obj);cJSON_Delete(obj);
 if(body){char path[90];snprintf(path,sizeof(path),"/api/services/%s/%s",c->domain,c->service);(void)call(path,body,NULL);free(body);}
}
static void task(void *arg){
 (void)arg;TickType_t last=0;
 for(;;){
  network_wifi_status_t wifi;network_service_get_wifi_status(&wifi);
  if(!wifi.connected){vTaskDelay(pdMS_TO_TICKS(1500));continue;}
  cmd_t c;
  if(xQueueReceive(q,&c,pdMS_TO_TICKS(100))==pdTRUE){send_cmd(&c);continue;}
  if(!last||(xTaskGetTickCount()-last)>pdMS_TO_TICKS(6000)){last=xTaskGetTickCount();refresh();}
 }
}
esp_err_t ha_devices_init(void){
 if(q)return ESP_OK;network_service_get_backend_config(&config);
 if(!config.ha_url[0]||!config.ha_token[0])return ESP_ERR_NOT_FOUND;
 lock=xSemaphoreCreateMutex();q=xQueueCreate(32,sizeof(cmd_t));if(!lock||!q)return ESP_ERR_NO_MEM;
 state.ac_temp_x2=48;state.ac_fan=4;state.bath_target_x10=250;state.bath_current_x10=200;
 if(xTaskCreate(task,"ha_devices",8192,NULL,3,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
 ESP_LOGI(TAG,"HA devices worker started");return ESP_OK;
}
