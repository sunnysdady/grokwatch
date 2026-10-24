// GrokWatch BLE peripheral: service 7a6e0001-4772-6f6b-5761-746368000001, RX write / TX notify, name "GrokWatch".
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#define TAG "GwBLE"
static uint16_t tx_handle;
static uint8_t own_addr_type;
extern void gw_on_write(const uint8_t*, uint16_t);
static const ble_uuid128_t svc_uuid = BLE_UUID128_INIT(0x01,0x00,0x00,0x68,0x63,0x74,0x61,0x57,0x6b,0x6f,0x72,0x47,0x01,0x00,0x6e,0x7a);
static const ble_uuid128_t rx_uuid  = BLE_UUID128_INIT(0x01,0x00,0x00,0x68,0x63,0x74,0x61,0x57,0x6b,0x6f,0x72,0x47,0x02,0x00,0x6e,0x7a);
static const ble_uuid128_t tx_uuid  = BLE_UUID128_INIT(0x01,0x00,0x00,0x68,0x63,0x74,0x61,0x57,0x6b,0x6f,0x72,0x47,0x03,0x00,0x6e,0x7a);
static int chr_access(uint16_t ch, uint16_t ah, struct ble_gatt_access_ctxt *ctxt, void *arg){
  if(ctxt->op==BLE_GATT_ACCESS_OP_WRITE_CHR){ uint8_t buf[512]; uint16_t n=0;
    ble_hs_mbuf_to_flat(ctxt->om,buf,sizeof buf,&n); gw_on_write(buf,n);
    ESP_LOGD(TAG,"rx %u",n);} return 0;}
static const struct ble_gatt_svc_def svcs[]={{.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&svc_uuid.u,
 .characteristics=(struct ble_gatt_chr_def[]){
  {.uuid=&rx_uuid.u,.access_cb=chr_access,.flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP},
  {.uuid=&tx_uuid.u,.access_cb=chr_access,.val_handle=&tx_handle,.flags=BLE_GATT_CHR_F_NOTIFY},{0}}},{0}};
static void advertise(void);
static uint16_t conn_h=0xFFFF;
static void notify(const uint8_t* d,int n){ if(conn_h==0xFFFF) return; struct os_mbuf* om=ble_hs_mbuf_from_flat(d,n); if(om) ble_gatts_notify_custom(conn_h,tx_handle,om);}
static int gap_ev(struct ble_gap_event *e, void *a){
  switch(e->type){
  case BLE_GAP_EVENT_CONNECT: if(e->connect.status) advertise(); else {conn_h=e->connect.conn_handle; ESP_LOGI(TAG,"connected");} break;
  case BLE_GAP_EVENT_DISCONNECT: conn_h=0xFFFF; ESP_LOGI(TAG,"disconnected"); advertise(); break;
  case BLE_GAP_EVENT_MTU: ESP_LOGI(TAG,"mtu=%d",e->mtu.value); break;
  case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
  default: break;}
  return 0;}
static void advertise(void){
  struct ble_hs_adv_fields f={0}; f.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
  f.uuids128=(ble_uuid128_t*)&svc_uuid; f.num_uuids128=1; f.uuids128_is_complete=1;
  int rc=ble_gap_adv_set_fields(&f);
  struct ble_hs_adv_fields r={0}; const char*n=ble_svc_gap_device_name(); r.name=(uint8_t*)n; r.name_len=strlen(n); r.name_is_complete=1;
  rc|=ble_gap_adv_rsp_set_fields(&r);
  struct ble_gap_adv_params p={0}; p.conn_mode=BLE_GAP_CONN_MODE_UND; p.disc_mode=BLE_GAP_DISC_MODE_GEN;
  rc|=ble_gap_adv_start(own_addr_type,NULL,BLE_HS_FOREVER,&p,gap_ev,NULL);
  ESP_LOGI(TAG,"advertise rc=%d name=%s",rc,n);}
static void on_sync(void){ ble_hs_id_infer_auto(0,&own_addr_type); advertise(); }
static void host_task(void*p){ nimble_port_run(); nimble_port_freertos_deinit(); }
void gw_ble_start(void){
  if(nimble_port_init()!=ESP_OK){ESP_LOGE(TAG,"nimble init fail");return;}
  ble_hs_cfg.sync_cb=on_sync; ble_svc_gap_init(); ble_svc_gatt_init();
  ble_gatts_count_cfg(svcs); ble_gatts_add_svcs(svcs); ble_svc_gap_device_name_set("GrokWatch");
  nimble_port_freertos_init(host_task); ESP_LOGI(TAG,"advertising as GrokWatch");}
bool gw_ble_link(void){ return conn_h!=0xFFFF; }
void gw_ble_notify(const uint8_t* d,int n){ notify(d,n); }
