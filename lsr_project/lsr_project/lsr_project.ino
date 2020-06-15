#include "dht.h"
#include <LiquidCrystal.h>
#include <Chrono.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <Fuzzy.h>

byte temp_icon[8] = {
       0b00100,
       0b00110,
       0b00100,
       0b00110,
       0b00100,
       0b01110,
       0b01110,
       0b00000
};
byte humidity_icon[8] = {
    0b00100,
    0b00100,
    0b01110,
    0b01110,
    0b10111,
    0b10111,
    0b01110,
    0b00000
};

#define DHT22PIN 7
#define FanGate 6
#define ONE_WIRE_BUS 13  // on pin 13 (a 4.7K resistor is necessary)

// Instantiating an objects
dht DHT22;
DeviceAddress thermometerAddress;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

LiquidCrystal lcd(12, 8, 5, 4, 3, 2);
Chrono ledChrono;
Chrono weatherChrono;

const int redpin = 11;
const int greenpin = 10;
const int bluepin = 9;

//Fuzzy* fuzzy = new Fuzzy();
Fuzzy* fuzzy2 = new Fuzzy();

// FuzzyInput fan
//FuzzySet *_very_cold = new FuzzySet(-55, -55, 0, 20);
//FuzzySet *_cold =      new FuzzySet(20, 30, 30, 40);
//FuzzySet *_warm =      new FuzzySet(30, 40, 40, 50);
//FuzzySet *_hot =       new FuzzySet(40, 50, 50, 60);
//FuzzySet *_very_hot =  new FuzzySet(50, 60, 125, 125);
//
// FuzzyOutput fan
//FuzzySet *_zero =      new FuzzySet(0, 0, 0, 0);
//FuzzySet *_slow =      new FuzzySet(45, 85, 85, 130);
//FuzzySet *_medium =    new FuzzySet(85, 130, 130, 170);
//FuzzySet *_fast =      new FuzzySet(130, 170, 170, 210);
//FuzzySet *_very_fast = new FuzzySet(210, 255, 255, 255);

//-------------------------------------------------------------------------

// FuzzyInput temp
//FuzzySet *low =     new FuzzySet(19.5, 19.5, 19.5, 22.5);
//FuzzySet *optimal = new FuzzySet(19.5, 22.5, 24.5, 27);
//FuzzySet *high =    new FuzzySet(24.5, 27, 27, 27);

FuzzySet *really_cold = new FuzzySet(-30, -30, 5, 7);
FuzzySet *cold =        new FuzzySet(6, 15, 15, 18);
FuzzySet *warm =        new FuzzySet(17, 22, 24, 27);
FuzzySet *hot =         new FuzzySet(25, 29, 29, 33);
FuzzySet *too_hot =     new FuzzySet(31, 34, 35, 35);

// FuzzyInput humidity
FuzzySet *humidity_critical_low =  new FuzzySet(0, 0, 15, 25);
FuzzySet *humidity_low =           new FuzzySet(20, 28, 28, 32);
FuzzySet *humidity_good =          new FuzzySet(30, 40, 55, 62);
FuzzySet *humidity_high =          new FuzzySet(60, 70, 70, 85);
FuzzySet *humidity_critical_high = new FuzzySet(80, 90, 100, 100);

// FuzzyOutput weather overall
FuzzySet *very_bad =  new FuzzySet(0, 0, 0, 8);
FuzzySet *bad =       new FuzzySet(8, 48, 48, 64);
FuzzySet *optimal =   new FuzzySet(64, 80, 80, 96);
FuzzySet *good =      new FuzzySet(96, 112, 112, 128);
FuzzySet *very_good = new FuzzySet(128, 144, 144, 160);

//-------------------------------------------------------------------------

static float toCelsius(float fromFahrenheit) { return (fromFahrenheit - 32.0) / 1.8; };
static float toFahrenheit(float fromCelcius) { return 1.8 * fromCelcius + 32.0; };

float computeHeatIndex(float temperature, float percentHumidity, bool isFahrenheit = false) {
    // Using both Rothfusz and Steadman's equations
    // http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
    float hi;

    if (!isFahrenheit) {
        temperature = toFahrenheit(temperature);
    }

    hi = 0.5 * (temperature + 61.0 + ((temperature - 68.0) * 1.2) + (percentHumidity * 0.094));

    if (hi > 79) {
        hi = -42.379 +
            2.04901523 * temperature +
            10.14333127 * percentHumidity +
            -0.22475541 * temperature * percentHumidity +
            -0.00683783 * pow(temperature, 2) +
            -0.05481717 * pow(percentHumidity, 2) +
            0.00122874 * pow(temperature, 2) * percentHumidity +
            0.00085282 * temperature * pow(percentHumidity, 2) +
            -0.00000199 * pow(temperature, 2) * pow(percentHumidity, 2);

        if ((percentHumidity < 13) && (temperature >= 80.0) && (temperature <= 112.0))
            hi -= ((13.0 - percentHumidity) * 0.25) * sqrt((17.0 - abs(temperature - 95.0)) * 0.05882);

        else if ((percentHumidity > 85.0) && (temperature >= 80.0) && (temperature <= 87.0))
            hi += ((percentHumidity - 85.0) * 0.1) * ((87.0 - temperature) * 0.2);
    }

    return isFahrenheit ? hi : toCelsius(hi);
}

float computeDewPoint(float temperature, float percentHumidity, bool isFahrenheit = false)
{
    // reference: http://wahiduddin.net/calc/density_algorithms.htm
    if (isFahrenheit) {
        temperature = toCelsius(temperature);
    }
    double A0 = 373.15 / (273.15 + (double)temperature);
    double SUM = -7.90298 * (A0 - 1);
    SUM += 5.02808 * log10(A0);
    SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / A0))) - 1);
    SUM += 8.1328e-3 * (pow(10, (-3.49149 * (A0 - 1))) - 1);
    SUM += log10(1013.246);
    double VP = pow(10, SUM - 3) * (double)percentHumidity;
    double Td = log(VP / 0.61078); // temp var
    Td = (241.88 * Td) / (17.558 - Td);
    return isFahrenheit ? toFahrenheit(Td) : Td;
}


void setup()
{
    Serial.begin(115200);
    initializeLCD();
    initializeLED();
    initializeFAN();
    initializeTempProbe();

    ////FuzzyInputSet1 for fan control
    //FuzzyInput *temperature = new FuzzyInput(1);
    //temperature->addFuzzySet(_very_cold);
    //temperature->addFuzzySet(_cold);
    //temperature->addFuzzySet(_warm);
    //temperature->addFuzzySet(_hot);
    //temperature->addFuzzySet(_very_hot);
    //fuzzy->addFuzzyInput(temperature);

    ////FuzzyOutputSet1 for fan control
    //FuzzyOutput *speed = new FuzzyOutput(1);
    //speed->addFuzzySet(_zero);
    //speed->addFuzzySet(_slow);
    //speed->addFuzzySet(_medium);
    //speed->addFuzzySet(_fast);
    //speed->addFuzzySet(_very_fast);
    //fuzzy->addFuzzyOutput(speed);

    //// Building FuzzyRule 1
    //FuzzyRuleAntecedent *ifTempVeryCold = new FuzzyRuleAntecedent();
    //ifTempVeryCold->joinSingle(_very_cold);
    //FuzzyRuleConsequent *thenZero = new FuzzyRuleConsequent();
    //thenZero->addOutput(_zero);
    //FuzzyRule *fanFuzzyRule1 = new FuzzyRule(1, ifTempVeryCold, thenZero);
    //fuzzy->addFuzzyRule(fanFuzzyRule1);

    //// Building FuzzyRule 2
    //FuzzyRuleAntecedent *ifTempCold = new FuzzyRuleAntecedent();
    //ifTempCold->joinSingle(_cold);
    //FuzzyRuleConsequent *thenSlow = new FuzzyRuleConsequent();
    //thenSlow->addOutput(_slow);
    //FuzzyRule *fanFuzzyRule2 = new FuzzyRule(2, ifTempCold, thenSlow);
    //fuzzy->addFuzzyRule(fanFuzzyRule2);

    //// Building FuzzyRule 3
    //FuzzyRuleAntecedent *ifTempWarm = new FuzzyRuleAntecedent();
    //ifTempWarm->joinSingle(_warm);
    //FuzzyRuleConsequent *thenMedium = new FuzzyRuleConsequent();
    //thenMedium->addOutput(_medium);
    //FuzzyRule *fanFuzzyRule3 = new FuzzyRule(3, ifTempWarm, thenMedium);
    //fuzzy->addFuzzyRule(fanFuzzyRule3);
    //
    //// Building FuzzyRule 4
    //FuzzyRuleAntecedent *ifTempHot = new FuzzyRuleAntecedent();
    //ifTempHot->joinSingle(_hot);
    //FuzzyRuleConsequent *thenFast = new FuzzyRuleConsequent();
    //thenFast->addOutput(_fast);
    //FuzzyRule *fanFuzzyRule4 = new FuzzyRule(4, ifTempHot, thenFast);
    //fuzzy->addFuzzyRule(fanFuzzyRule4);

    //// Building FuzzyRule 5
    //FuzzyRuleAntecedent *ifTempVeryHot = new FuzzyRuleAntecedent();
    //ifTempVeryHot->joinSingle(_very_hot);
    //FuzzyRuleConsequent *thenVeyFast = new FuzzyRuleConsequent();
    //thenVeyFast->addOutput(_very_fast);
    //FuzzyRule *fanFuzzyRule5 = new FuzzyRule(5, ifTempVeryHot, thenVeyFast);
    //fuzzy->addFuzzyRule(fanFuzzyRule5);

    //----------------------------------------------------------------------------

    //FuzzyInputSet1 weather_temperature
    FuzzyInput *weather_temperature = new FuzzyInput(1);
    weather_temperature->addFuzzySet(really_cold);
    weather_temperature->addFuzzySet(cold);
    weather_temperature->addFuzzySet(warm);
    weather_temperature->addFuzzySet(hot);
    weather_temperature->addFuzzySet(too_hot);
    fuzzy2->addFuzzyInput(weather_temperature);
    
    // FuzzyInputSet2 humidity
    FuzzyInput *humidity = new FuzzyInput(2);
    humidity->addFuzzySet(humidity_critical_low);
    humidity->addFuzzySet(humidity_low);
    humidity->addFuzzySet(humidity_good);
    humidity->addFuzzySet(humidity_high);
    humidity->addFuzzySet(humidity_critical_high);
    fuzzy2->addFuzzyInput(humidity);

    //FuzzyOutputSet1 for weather_overall
    FuzzyOutput *weather_overall = new FuzzyOutput(1);
    weather_overall->addFuzzySet(very_bad);
    weather_overall->addFuzzySet(bad);
    weather_overall->addFuzzySet(optimal);
    weather_overall->addFuzzySet(good);
    weather_overall->addFuzzySet(very_good);
    fuzzy2->addFuzzyOutput(weather_overall);

    //FuzzyOutputSet1 for fan control
    /*FuzzyOutput *speed = new FuzzyOutput(2);
    speed->addFuzzySet(_zero);
    speed->addFuzzySet(_slow);
    speed->addFuzzySet(_medium);
    speed->addFuzzySet(_fast);
    speed->addFuzzySet(_very_fast);
    fuzzy2->addFuzzyOutput(speed);*/

    // improved MM

    // very_bad
    FuzzyRuleAntecedent* deadly_humidity = new FuzzyRuleAntecedent();
    deadly_humidity->joinWithOR(humidity_critical_low, too_hot);
    FuzzyRuleAntecedent* deadly_humidity2 = new FuzzyRuleAntecedent();
    deadly_humidity2->joinWithOR(deadly_humidity, really_cold);
    FuzzyRuleAntecedent* deadly_humidity3 = new FuzzyRuleAntecedent();
    deadly_humidity3->joinWithOR(deadly_humidity2, humidity_critical_high);

    FuzzyRuleConsequent* dont_go_outside = new FuzzyRuleConsequent();
    dont_go_outside->addOutput(very_bad);
    //dont_go_outside->addOutput(_very_fast);

    FuzzyRule* fuzzyRule1 = new FuzzyRule(1, deadly_humidity3, dont_go_outside);
    fuzzy2->addFuzzyRule(fuzzyRule1);

    // bad
    FuzzyRuleAntecedent* if_hot_and_high = new FuzzyRuleAntecedent();
    if_hot_and_high->joinWithAND(hot, humidity_high);
    FuzzyRuleAntecedent* if_cold_and_low = new FuzzyRuleAntecedent();
    if_cold_and_low->joinWithAND(cold, humidity_low);
    FuzzyRuleAntecedent* thisORthat = new FuzzyRuleAntecedent();
    thisORthat->joinWithOR(if_hot_and_high, if_cold_and_low);
    FuzzyRuleConsequent* not_good = new FuzzyRuleConsequent();
    not_good->addOutput(bad);
    FuzzyRule* fuzzyRule2 = new FuzzyRule(2, thisORthat, not_good);
    fuzzy2->addFuzzyRule(fuzzyRule2);

    // optimal
    FuzzyRuleAntecedent* if_high_or_okay = new FuzzyRuleAntecedent();
    if_high_or_okay->joinWithOR(humidity_good, humidity_high);
    FuzzyRuleAntecedent* if_cold_and_high_or_okay = new FuzzyRuleAntecedent();
    if_cold_and_high_or_okay->joinWithAND(cold, if_high_or_okay);
    FuzzyRuleAntecedent* if_hot_and_low = new FuzzyRuleAntecedent();
    if_hot_and_low->joinWithAND(hot, humidity_low);
    FuzzyRuleAntecedent* thisORthat2 = new FuzzyRuleAntecedent();
    thisORthat2->joinWithOR(if_cold_and_high_or_okay, if_hot_and_low);
    FuzzyRuleConsequent* okay = new FuzzyRuleConsequent();
    okay->addOutput(optimal);
    FuzzyRule* fuzzyRule3 = new FuzzyRule(3, thisORthat2, okay);
    fuzzy2->addFuzzyRule(fuzzyRule3);

    // good
    FuzzyRuleAntecedent* check_bad_humidity = new FuzzyRuleAntecedent();
    check_bad_humidity->joinWithOR(humidity_high, humidity_low);
    FuzzyRuleAntecedent* if_weather_warm_and_bad_humidity = new FuzzyRuleAntecedent();
    if_weather_warm_and_bad_humidity->joinWithAND(check_bad_humidity, warm);
    FuzzyRuleAntecedent* if_weather_hot_and_good_humidity = new FuzzyRuleAntecedent();
    if_weather_hot_and_good_humidity->joinWithAND(humidity_good, hot);
    FuzzyRuleAntecedent* this_or_that3 = new FuzzyRuleAntecedent();
    this_or_that3->joinWithOR(if_weather_hot_and_good_humidity, if_weather_warm_and_bad_humidity);
    FuzzyRuleConsequent* good_already = new FuzzyRuleConsequent();
    good_already->addOutput(good);
    FuzzyRule* fuzzyRule4 = new FuzzyRule(4, this_or_that3, good_already);
    fuzzy2->addFuzzyRule(fuzzyRule4);

    // very_good
    FuzzyRuleAntecedent* if_weather_warm_and_humidity_good = new FuzzyRuleAntecedent();
    if_weather_warm_and_humidity_good->joinWithAND(warm, humidity_good);
    FuzzyRuleConsequent* best = new FuzzyRuleConsequent();
    best->addOutput(very_good);
    FuzzyRule* fuzzyRule5 = new FuzzyRule(5, if_weather_warm_and_humidity_good, best);

    fuzzy2->addFuzzyRule(fuzzyRule5);
}

void loop()
{
    if (weatherChrono.hasPassed(500)){
        float temp, humidity, dew_point, heat_index;
        
        temp_sensor(temp);
        humidity_sensor(humidity);

        dew_point = computeDewPoint(temp, humidity);
        Serial.print("Punkt rosy: ");
        Serial.print(dew_point);
        dew_point_response(dew_point);

        heat_index = computeHeatIndex(temp, humidity);
        Serial.print("Heat index: ");
        Serial.print(heat_index);
        heat_index_response(heat_index);

        // for fan 
        //fuzzy->setInput(1, temp);
        
        // for comfort
        fuzzy2->setInput(1, temp);
        fuzzy2->setInput(2, humidity);

        //fuzzy->fuzzify();
        fuzzy2->fuzzify();

        /*float fan_control = fuzzy->defuzzify(1);
        fan(fan_control);*/
        
        float comfort = fuzzy2->defuzzify(1);
        //float fan_control = fuzzy2->defuzzify(2);

        
        diode(comfort);
        fan(comfort, temp);

        weatherChrono.restart();
    }
}
void initializeTempProbe()
{
    tempSensor.begin();

    if (!tempSensor.getAddress(thermometerAddress, 0))
        Serial.println("Unable to find Device.");
    else {
        Serial.print("Device 0 Address: ");
        printAddress(thermometerAddress);
        Serial.println();
    }

    tempSensor.setResolution(thermometerAddress, 12);
}

void initializeLCD()
{
    lcd.begin(16, 2);
    lcd.createChar(1, temp_icon);
    lcd.createChar(2, humidity_icon);
}

void initializeLED()
{
    pinMode(redpin, OUTPUT);
    pinMode(bluepin, OUTPUT);
    pinMode(greenpin, OUTPUT);
}

void initializeFAN()
{
    pinMode(FanGate, OUTPUT);
    digitalWrite(FanGate, LOW);
}

void humidity_sensor(float &humidity)
{
    DHT22.read(DHT22PIN);

    //temp = DHT22.temperature;
    humidity = DHT22.humidity;
    
    //Serial.print("Temperatura (C): ");
    //Serial.println(temp, 2);

    Serial.print("Wilgotnosc (%): ");
    Serial.println(humidity, 2);

    //lcd.write(1);
    //lcd.setCursor(2, 0);
    //lcd.print(temp, 0);
    //lcd.print((char)223); //degree sign
    //lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.write(2);
    lcd.setCursor(2, 1);
    lcd.print(humidity, 0);
    lcd.print("%");
}

void temp_sensor(float& temp)
{
    tempSensor.requestTemperatures();
    temp = tempSensor.getTempC(thermometerAddress);

    Serial.print("Temperatura (C): ");
    Serial.println(temp, 2);

    lcd.setCursor(0, 0);
    lcd.write(1);
    lcd.setCursor(2, 0);
    lcd.print(temp, 1);
    lcd.print((char)223); //degree sign
    lcd.print("C");
}

void fan(float fan_control, float temp)
{
    float  speed;
    Serial.print("Fan speed: ");
    speed = temp < 20 ?  0 : map(fan_control, 160, 0, 0, 255);
    analogWrite(FanGate, speed);
    Serial.println(speed);
}

void diode(float comfort) 
{
    Serial.print("Comfort: ");
    Serial.print(comfort);
    showAnalogRGB(CHSV(comfort, 255, 255));

    Serial.print(" -> ");

    if (comfort < 8) Serial.println("very_bad");
    else if (comfort < 64) Serial.println("bad");
    else if (comfort < 96) Serial.println("optimal");
    else if (comfort < 128) Serial.println("good");
    else if (comfort <= 160) Serial.println("very_good");

}

// print device address from the address array
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void showAnalogRGB(const CRGB& rgb)
{
    analogWrite(redpin, rgb.r);
    analogWrite(greenpin, rgb.g);
    analogWrite(bluepin, rgb.b);
}

void dew_point_response(float dew_point)
{
    Serial.print(" -> ");

    if (dew_point > 26) Serial.println("deadly");
    else if (dew_point < 10) Serial.println("too dry");
    else if (dew_point < 12) Serial.println("very comfortable");
    else if (dew_point < 16) Serial.println("comfortable");
    else if (dew_point < 18) Serial.println("OK");
    else if (dew_point < 21) Serial.println("somewhat unconfortable");
    else if (dew_point < 24) Serial.println("quite unconfortable");
    else if (dew_point <= 26) Serial.println("extremely unconfortable");

}

void heat_index_response(float heat_index)
{
    Serial.print(" -> ");

    if (heat_index > 54) Serial.println("heat stroke");
    else if (heat_index > 45) Serial.println("dangerous");
    else if (heat_index < 29) Serial.println("no discomfort");
    else if (heat_index <= 39) Serial.println("some discomfort");
    else if (heat_index <= 45) Serial.println("great discomfort");
}