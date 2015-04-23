/*
Garage Door Opener, v1, 04/21/2015
by Sesh Kanury

Anyone is free to use this software as long as credit is given to the author
and it is only used for means of a good nature.

Eat my shorts.
*/

#define DOOR_BUTTONS 3;





int Up_Pin = D0;
int Down_Pin = D1;
int Stop_Pin = D2;
int Upper_Limit_Pin = D4;
int Lower_Limit_Pin = D5;

int last_Upper_Limit_state;
int last_Lower_Limit_state;

unsigned long last_publish = 0;
unsigned long publish_interval = 60000;
unsigned long last_change_publish = 0;
unsigned long change_publish_interval = 750;





int pinArraySize = 22;

struct structEEPROM {
    double crc;           //cycle redundancy check - if memory is not intact, set all to defaults
    int adminPin;
    int pinArray[22];
};

union {
    structEEPROM settings;
    char eeArray[sizeof(structEEPROM)];
} EEPROMData;







//declare spark function
int get_web_message(String command);













/************************************ stamdard SETUP ************************************/
void setup() {
    Serial.begin(9600);
    
    //register spark function
    Spark.function("from_web", get_web_message);
    Spark.variable("pinArray", EEPROMData.settings.pinArray, INT);
    
    pinMode(Up_Pin, OUTPUT);
    pinMode(Down_Pin, OUTPUT);
    pinMode(Stop_Pin, OUTPUT);
    pinMode(Upper_Limit_Pin, INPUT);
    pinMode(Lower_Limit_Pin, INPUT);
    
    digitalWrite(Up_Pin, LOW);
    digitalWrite(Down_Pin, LOW);
    digitalWrite(Stop_Pin, LOW);
    
    readEEPROM();
    //EEPROMData.settings.adminPin = 99999;
    if(EEPROMData.settings.crc != 1234.56){
        setDefaultEEPROM();
    }
    consolidate_access_pins();
    writeEEPROM();
}
/****************************************************************************************/














/************************************ standard LOOP *************************************/
void loop() {
    unsigned long now = millis();
    
    if(now-last_change_publish > change_publish_interval){
        last_change_publish = now;    
        if(check_for_limit_switch_change()) publish_door_state();
    }
    
    if(now-last_publish > publish_interval){
        last_publish = now;
        publish_door_state();
    }
}
/****************************************************************************************/
















/****************************** Detect Limit Switch Change ******************************/
int check_for_limit_switch_change(){
    int upper_state = digitalRead(Upper_Limit_Pin);
    int lower_state = digitalRead(Lower_Limit_Pin);
    int result = 0;
    
    if(upper_state != last_Upper_Limit_state || lower_state != last_Lower_Limit_state) result = 1;
    
    last_Upper_Limit_state = upper_state;
    last_Lower_Limit_state = lower_state;
    
    return result;
}
/****************************************************************************************/





















/************************************ publish to web ************************************/
void publish_door_state(){
    String message = update_door_state();
    Spark.publish("doorState", message);
    
    Serial.println(message);
}
/****************************************************************************************/















/********************************** update door state ***********************************/
String update_door_state(){
    int upper_state = digitalRead(Upper_Limit_Pin);
    int lower_state = digitalRead(Lower_Limit_Pin);

    if(!upper_state && !lower_state) return "{\"state\":\"Half\"}";
    if(upper_state && !lower_state) return "{\"state\":\"Open\"}";
    if(!upper_state && lower_state) return "{\"state\":\"Closed\"}";
    if(upper_state && lower_state) return "{\"state\":\"Error\"}";
    
    return "{\"state\":\"Debug\"}";
}
/****************************************************************************************/















/************************************** do web com **************************************/
//All pin numbers must be 5 digits
//This expects a string beginning with a pin number to identify the user.
//The next character should be a character representing the action, as follows:
//<pin>O = open the door
//<pin>C = close the door
//<pin>S = stop the door
//<adminPin>D<pin> = delete <pin> from users
//<adminPin>A<pin> = add <pin> to users
//<adminPin>L<pin> = list all pins
//<adminPin>N<pin> = change admin pin to <pin>
int get_web_message(String command){
    Serial.print("Command:   ");Serial.println(command);
    //declared in the order they are in String command
    char command_pin[5];
    char command_character;
    char command_new_pin[5];
    
    int pin;
    int new_pin;
    
    unsigned int k;
    for(k=0; k<sizeof(command_pin); k++){
        command_pin[k] = 0;
        command_new_pin[k] = 0;
    }
    
    //parse and convert the recieved user pin to and int
    for(k=0; k<sizeof(command_pin); k++) command_pin[k] = command[k];
    pin = atoi(command_pin);
    Serial.print("ATOI pin:   "); Serial.println(pin);
    //parse the recieved command character
    command_character = command[5];
    Serial.print("command_character:   ");Serial.println(command_character);
    //0=wrong pin or no pin
    //1=user pin
    //2=admin pin
    int validation = validate_pin(pin);
    Serial.print("Validation:   "); Serial.println(validation);
    switch(validate_pin(pin)){
        case 0:
            break;
        case 1:
            Serial.println("USER");
            control_door(command_character);
            break;
        case 2:
            //Serial.println("ADMIN");
            //parse and convert the recieved NEW user pin to and int if it exists
            //otherwise abort
            if(sizeof(command) > (sizeof(command_pin)+1)){
                for(k=0; k<sizeof(command_new_pin); k++) command_new_pin[k] = command[k+6];
                new_pin = atoi(command_new_pin);
                Serial.print("new_pin:   "); Serial.println(new_pin);
            }else return -1;
            manage_access(command_character, new_pin);
            break;
        default:
            break;
    }
    
    
    unsigned long now = millis();
    if(now-last_publish > 1000){
        last_publish = now;
        publish_door_state();
    }
    
    writeEEPROM();
    
    return 0;
}
/****************************************************************************************/















/*********************************** control the door ***********************************/
void control_door(char command){
    Serial.print("control_door:   "); Serial.println(command);
    switch(command){
                case 'O':
                    Serial.println("opening");
                    digitalWrite(Up_Pin, HIGH);
                    delay(500);                     //slow things down so it looks like a human pushed the button.
                    digitalWrite(Up_Pin, LOW);
                    break;
                case 'C':
                    Serial.println("closing");
                    digitalWrite(Down_Pin, HIGH);
                    delay(500);                     //slow things down so it looks like a human pushed the button.
                    digitalWrite(Down_Pin, LOW);    
                    break;
                case 'S':
                    Serial.println("stopping");
                    digitalWrite(Stop_Pin, HIGH);
                    delay(500);                     //slow things down so it looks like a human pushed the button.
                    digitalWrite(Stop_Pin, LOW); 
                    break;
                default:
                    Serial.println("nothing");
                    break;
            }
}
/****************************************************************************************/














/************************************* validate pin *************************************/
int validate_pin(int pin){
    Serial.print("validate_pin:   ");Serial.println(pin);
    Serial.print("Admin PIN:   "); Serial.println(EEPROMData.settings.adminPin);
    //unsigned int k = 0;
    int search_result = -1;
    //Serial.print("adminPin:   ");Serial.println(EEPROMData.settings.adminPin);
    
    if(pin == EEPROMData.settings.adminPin) return 2;
        else search_result = search_for_matching_pin(pin);
    if(search_result >= 0) return 1;
        else return 0;
}
/****************************************************************************************/














/******************************* Admin Access Management ********************************/
void manage_access(char command, int new_pin){
    Serial.println(command);
    Serial.println(new_pin);
    
    int search_result;
    int k = 0;
    char publish_pin[63];
    for(k=0;k<sizeof(publish_pin);k++) publish_pin[k] = 0;
    
    switch(command){
            case 'D':
                search_result = search_for_matching_pin(new_pin);
                if(search_result != -1){
                    EEPROMData.settings.pinArray[search_result] = 0;
                    consolidate_access_pins();
                }
                break;
            case 'A':
                search_result = search_for_matching_pin(0);
                if(new_pin != 0 &&  search_for_matching_pin(new_pin) == -1 && search_result != -1)
                    EEPROMData.settings.pinArray[search_result] = new_pin;
                break;
            case 'L':
                for(k=0;k<pinArraySize;k++){
                    sprintf(publish_pin, "{\"pin\":\"%i\"}", EEPROMData.settings.pinArray[k]);
                    Spark.publish("pin_list", publish_pin);
                    Serial.print(EEPROMData.settings.pinArray[k]); Serial.print("   "); Serial.println(publish_pin);
                }
                break;
            case 'N':
                if(new_pin > 9999 || new_pin < 100000)
                    EEPROMData.settings.adminPin = new_pin;
                break;
            case 'Z':
                setDefaultEEPROM();
                break;
                
        }
}
/****************************************************************************************/












/*********************************** search for PINs ************************************/
int search_for_matching_pin(int pin){
    int k;
    for(k=0; k<pinArraySize;k++){
        if(pin == EEPROMData.settings.pinArray[k]) return k;
    }
    return -1;
}
/****************************************************************************************/













/*********************************** consolidate pins ***********************************/
//this puts all the pins in the array in the first cells available
//then fills the rest of the array with zeros
void consolidate_access_pins(){
    unsigned int k = 0;
    int index_counter = 0;
    int temp_array[pinArraySize];
    for(k=0;k<pinArraySize;k++) temp_array[k] = EEPROMData.settings.pinArray[k];

    
    //store the non-zero numbers in sequence from index 0
    for(k=0; k<pinArraySize;k++){
        if(temp_array[k]>9999 && temp_array[k]<100000){
            EEPROMData.settings.pinArray[index_counter] = temp_array[k];
            index_counter++;
        }
    }
    //handle array over-count reference    
    if(index_counter < pinArraySize)
        //fill the remaining cells with zeros
        for(k=index_counter; k<pinArraySize; k++){
            EEPROMData.settings.pinArray[k] = 0;
        }
}
/****************************************************************************************/

















/*********************************** EEPROM functions ***********************************/
void readEEPROM(){
    for(unsigned int i=0; i<sizeof(structEEPROM); i++){
        EEPROMData.eeArray[i] = EEPROM.read(i);
    }
}

void writeEEPROM(){
    for(unsigned int i=0; i<sizeof(structEEPROM); i++){
        EEPROM.write(i, EEPROMData.eeArray[i]);
    }
}

void setDefaultEEPROM(){
    EEPROMData.settings.crc = 1234.56;
    unsigned int k=0;
    for(k=0;k<pinArraySize;k++) EEPROMData.settings.pinArray[k] = 0;
    EEPROMData.settings.adminPin = 11111;
    
    writeEEPROM();
}
/****************************************************************************************/
