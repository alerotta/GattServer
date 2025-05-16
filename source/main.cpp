#include "GattCharacteristic.h"
#include "GattServer.h"
#include "GattService.h"
#include "PinNameAliases.h"
#include "ThisThread.h"
#include "mbed.h"
#include "platform/Callback.h"
#include "events/EventQueue.h"
#include "ble/BLE.h"
#include "gatt_server_process.h"
#include "mbed-trace/mbed_trace.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <utility>
#include "TextLCD.h"


AnalogIn alcohol_sensor(A5);
TextLCD lcd(D15, D14, D4, D5, D6, D8);
UnbufferedSerial pc(USBTX, USBRX, 115200); 

const float VREF = 5;   
const float RL = 15000.0;
const float R0 = 78544.84;
const float m =  -0.377;
const float b = 0.60;



class AlcoholService : public ble::GattServer::EventHandler {

template<typename T>
class ReadWriteNotifyIndicateCharacteristic : public GattCharacteristic {
public:
    
    ReadWriteNotifyIndicateCharacteristic(const UUID & uuid, const T& initial_value) :
        GattCharacteristic(
            uuid,
            reinterpret_cast<uint8_t*>(&_value),
            sizeof(_value),
            sizeof(_value),
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                             GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                             GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                             GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
            nullptr,
            0,
            false
        ),
        _value(initial_value) {
    }

    ble_error_t get(GattServer &server, T& dst) const
    {
        uint16_t value_length = sizeof(T);
        return server.read(getValueHandle(), reinterpret_cast<uint8_t*>(&dst), &value_length);
    }

 
    ble_error_t set(GattServer &server, const T& value, bool local_only = false) const
    {
        return server.write(getValueHandle(), reinterpret_cast<const uint8_t*>(&value), sizeof(T), local_only);
    }

private:
    T _value;  
};


   private: 
    GattServer *_server = nullptr;
    events::EventQueue *_event_queue = nullptr;

    GattService _alcohol_service;
    GattCharacteristic *_alcohol_characteristics[2];

    ReadWriteNotifyIndicateCharacteristic<float> _ppm_char;
    ReadWriteNotifyIndicateCharacteristic<float> _bac_char;



    public:
    
    AlcoholService() :
        _ppm_char("485f4145-52b9-4644-af1f-7a6b9322490f", 0),
        _bac_char("0a924ca7-87cd-4699-a3bd-abdcd9cf126a", 0),
        _alcohol_service(
            "51311102-030e-485f-b122-f8f381aa84ed",
            _alcohol_characteristics,
            sizeof(_alcohol_characteristics) / sizeof(_alcohol_characteristics[0])
        )
    {
        
        _alcohol_characteristics[0] = &_ppm_char;
        _alcohol_characteristics[1] = &_bac_char;


    }

    void start(BLE &ble, events::EventQueue &event_queue)
    {
        _server = &ble.gattServer();
        _event_queue = &event_queue;

        printf("Registering demo service\r\n");
        ble_error_t err = _server->addService(_alcohol_service);

        if (err) {
            printf("Error %u during demo service registration.\r\n", err);
            return;
        }

       
        _server->setEventHandler(this);


     
        printf("alcohol service registered\r\n");
        printf("service handle: %u\r\n", _alcohol_service.getHandle());
        printf("ppm characteristic value handle %u\r\n", _ppm_char.getValueHandle());
        printf("bac characteristic value handle %u\r\n", _bac_char.getValueHandle());


        
        _event_queue->call_every(9000ms, callback(this, &AlcoholService::update_alcohol));
    }


    
private:
    
    void onDataSent(const GattDataSentCallbackParams &params) override
    {
        printf("sent updates\r\n");
    }

    
    void onDataRead(const GattReadCallbackParams &params) override
    {
        printf("data read:\r\n");
        printf("connection handle: %u\r\n", params.connHandle);
        printf("attribute handle: %u", params.handle);
        if (params.handle == _ppm_char.getValueHandle()) {
            printf(" (hour characteristic)\r\n");
        } else if (params.handle == _bac_char.getValueHandle()) {
            printf(" (minute characteristic)\r\n");
        } else {
            printf("\r\n");
        }
    }

    void onUpdatesEnabled(const GattUpdatesEnabledCallbackParams &params) override
    {
        printf("update enabled on handle %d\r\n", params.attHandle);
    }


    void onUpdatesDisabled(const GattUpdatesDisabledCallbackParams &params) override
    {
        printf("update disabled on handle %d\r\n", params.attHandle);
    }


    void onConfirmationReceived(const GattConfirmationReceivedCallbackParams &params) override
    {
        printf("confirmation received on handle %d\r\n", params.attHandle);
    }


    private:


    std::pair<float,float> get_ppmbac (){
        float sensor_voltage = alcohol_sensor.read() * VREF ;
        float Rs = RL * ((5 - sensor_voltage) / sensor_voltage);
        float ratio = Rs / R0;
        float ppm = pow(10, ((log10(ratio) - b) / m));
        float BAC = ppm * 0.00021;
        return std::make_pair(ppm, BAC);
    }
   

    void update_alcohol(){
    
    //lcd.cls();
    std::pair<float,float> mean_val;

    for (int i = 3 ; i > 0; i--){
        //lcd.printf("Preparati: %i \n\n", i);
        printf("Preparati: %i \n\n", i);
        ThisThread::sleep_for(1000ms);

    }
    //lcd.cls();
    //lcd.printf("Soffia\n\n");
    printf("Soffia\n\n");

    for ( int j = 0 ;  j < 30; j++){
        std::pair<float, float> values = get_ppmbac();
        mean_val.first += values.first;
        mean_val.second += values.second;

        if (j == 29){
            mean_val.first /= 30;
            mean_val.second /= 30;
        }

        ThisThread::sleep_for(100ms);

    }

    
    ble_error_t err = _ppm_char.set(*_server, mean_val.first);
    if (err){
        printf("error %u updating ppm value\r\n", err);
        return;
    }

    err = _bac_char.set(*_server, mean_val.second);
    if (err){
        printf("error %u updating bac value\r\n", err);
        return;
    }

    printf("Alcohol PPM: %f, BAC: %f\n",  mean_val.first, mean_val.second );

    //lcd.printf("PPM: %.3f\n",mean_val.first);
    //lcd.printf("BAC: %.3f\n",mean_val.second);

    //printf("PPM: %.3f\n",mean_val.first);
    //printf("BAC: %.3f\n",mean_val.second);
    
    mean_val.first = 0;
    mean_val.second = 0;
       
}



};

float read_sensor_voltage() {
    return alcohol_sensor.read() * VREF;
}

float calculate_Rs(float voltage) {
    if (voltage < 0.01f) voltage = 0.01f; // Evita divisione per zero
    return RL * ((VREF - voltage) / voltage);
}


int main (){
    
    

      /*
    
    printf("\n--- Calibrazione MQ-3 (Nucleo F401RE - Mbed OS) ---\n");
    wait_us(2000000); // 2s delay

    float sum_Rs = 0.0f;

    for (int i = 0; i < 100; i++) {
        float voltage = read_sensor_voltage();
        float Rs = calculate_Rs(voltage);
        sum_Rs += Rs;

        printf("Lettura %d: V=%.3f V, Rs=%.2f Ohm\n", i+1, voltage, Rs);
        thread_sleep_for(100); // Attesa 100 ms
    }

    float avg_Rs = sum_Rs / 100;

    // Calibrazione in aria pulita (senza alcol): R0 = Rs
    float R0 = avg_Rs;

    printf("\n--- Risultati Calibrazione ---\n");
    printf("Rs medio: %.2f Ohm\n", avg_Rs);
    printf("R0 calcolato (aria pulita): %.2f Ohm\n", R0);

     while (true) {
        thread_sleep_for(1000); // loop fermo
    }

    */

 

    

     // controllo schermo accensione

    //lcd.setCursor(TextLCD_Base::CurOff_BlkOff);
    //lcd.printf("Attendi\n\n");
    printf("Attendi\n\n");
    ThisThread::sleep_for(10000ms);
    for (int i = 9 ; i > 0; i--){
        //lcd.printf("Attendi %i\n\n", i);
        printf("Attendi %i\n\n", i);
        ThisThread::sleep_for(1000ms);
    }
    //lcd.cls();
    //lcd.printf("Avviamento\n\n");

    printf("Avviamento\n\n");


    mbed_trace_init();

    BLE &ble = BLE::Instance();
    events::EventQueue event_queue;
    AlcoholService demo_service;

    
    GattServerProcess ble_process(event_queue, ble);

    
    ble_process.on_init(callback(&demo_service, &AlcoholService::start));
    ble_process.start();

    
    

    return 0;
}











