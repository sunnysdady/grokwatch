#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// GrokWatch voice to-do: fetch / check-off against the xiaozhi-server gw_todo plugin (same host:port as the OTA URL).
// One short-lived task per request (no always-running task); nothing runs unless the To-do page is opened or a row is tapped.
#include <cstring>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <cJSON.h>
#include "board.h"
#include "settings.h"
#include "gw_todo_net.h"
#define TAG "GwTodo"
gw_todo_list_t gw_todo = { .state = 1 };
static volatile bool busy = false;
static void (*done_cb)() = nullptr;
static std::string base_url(){
  Settings s("wifi", false); std::string u = s.GetString("ota_url"); if(u.empty()) u = CONFIG_OTA_URL;
  size_t p = u.find("://"); p = (p==std::string::npos)?0:p+3; size_t e = u.find('/', p);
  return u.substr(0, e==std::string::npos?u.size():e);   // "http://host:port"
}
static void parse(const std::string& body){
  cJSON* j = cJSON_Parse(body.c_str()); if(!j){ gw_todo.state = 2; return; }
  gw_todo_list_t t = {}; cJSON* it = cJSON_GetObjectItem(j,"items"); cJSON* o = cJSON_GetObjectItem(j,"open");
  t.open = cJSON_IsNumber(o)?o->valueint:0; cJSON* x;
  cJSON_ArrayForEach(x, it){ if(t.n>=GW_TODO_ROWS) break; cJSON* id=cJSON_GetObjectItem(x,"id"); cJSON* tx=cJSON_GetObjectItem(x,"text");
    if(!cJSON_IsString(id)||!cJSON_IsString(tx)) continue;
    strncpy(t.id[t.n], id->valuestring, sizeof t.id[0]-1); gw_utf8_copy(t.text[t.n], sizeof t.text[0], tx->valuestring, (int)strlen(tx->valuestring)); t.n++; }
  cJSON_Delete(j); t.state = 0; gw_todo = t;
}
struct Req { char method[6]; std::string path; };
static void task(void* a){
  Req* r = (Req*)a; std::string url = base_url() + r->path;
  auto http = Board::GetInstance().GetNetwork()->CreateHttp(0); http->SetTimeout(4000);
  if(http->Open(r->method, url) && http->GetStatusCode()==200){ parse(http->ReadAll()); } else { gw_todo.state = 2; ESP_LOGW(TAG,"%s %s failed", r->method, r->path.c_str()); }
  http->Close(); delete r; busy = false; if(done_cb) done_cb(); vTaskDelete(nullptr);
}
static void start(const char* m, std::string path){
  if(busy) return; busy = true; Req* r = new Req(); strcpy(r->method, m); r->path = path;
  if(xTaskCreate(task, "gw_todo", 6144, r, 3, nullptr) != pdPASS){ delete r; busy = false; }
}
void GwTodoSetDoneCb(void (*f)()){ done_cb = f; }
void GwTodoFetch(){ if(gw_todo.n==0) gw_todo.state = 1; start("GET", "/gw/todo?n=" + std::to_string(GW_TODO_ROWS)); }
void GwTodoCheck(int row){ if(row<0||row>=gw_todo.n||gw_todo.done[row]) return; gw_todo.done[row] = true;   // optimistic
  start("POST", std::string("/gw/todo/done?n=") + std::to_string(GW_TODO_ROWS) + "&id=" + gw_todo.id[row]); }
