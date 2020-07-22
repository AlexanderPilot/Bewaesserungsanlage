//Einbinden von Bibliotheken
#include <Arduino.h>
#include "ESP32_MailClient.h"

//Definition globale Variablen
#define TIME_TO_SLEEP 30       //Zeit in Sekunden wie lange der ESP32 in den Deep-Sleep geht
#define uS_TO_S_FACTOR 1000000 //Umrechnungsfaktor von Mikrosekunden in Sekunden

//WiFi Einstellungen
const char *ssid = "OnLine";
const char *password = "Br8#Pojg56";

//Mail Einstellungen
#define emailSenderAccount "mailbot.pilot@gmail.com"
#define emailSenderPassword "Ale8and6r"
#define emailRecipient "alexander.pilot@gmx.de"
#define smtpServer "smtp.gmail.com"
#define smtpServerPort 465

//Pins für Peripherie
#define PIN_SOILSENS 11
#define PIN_PUMP 12
#define PIN_ECHO 5
#define PIN_TRIGGER 2

//Deepsleep sichere Speicherung von Variablen
RTC_DATA_ATTR int bootCount = 0;

//Anlegen von Objekten
SMTPData smtpData;

//Definition Funktionen

/***************************************************************************
 * Funktion zum Auslesen des Email Sende-Status
 * in: kein
 * out: kein
 **************************************************************************/
void sendCallback(SendStatus msg)
{
    // Print the current status
    Serial.println(msg.info());

    // Do something when complete
    if (msg.success())
    {
        Serial.println("----------------");
    }
}

/***************************************************************************
 * Funktion zum Auslesen des Aufwachgrunds
 * in: kein
 * out: kein
 **************************************************************************/
void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
    case 1:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case 2:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case 3:
        Serial.println("Wakeup caused by xx");
        break;
    case 4:
        Serial.println("Wakeup caused by timer");
        break;
    case 5:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.println("Wakeup was not caused by deep sleep");
        break;
    }
}

void send_mail(uint8_t reason)
{
    Serial.print("Connecting to WiFi");

    //Initialisiere WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(200);
    }
    Serial.println();
    Serial.println("WiFi connected.");
    Serial.println();

    //Initialisiere Email
    Serial.println("Preparing to send email");
    Serial.println();
    smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword); // Set the SMTP Server Email host, port, account and password
    smtpData.setSender("Bewaesserungsanlage", emailSenderAccount);                          // Set the sender name and Email
    smtpData.setPriority("Normal");                                                         // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)

    //Elemente für Email erstellen
    switch (reason)
    {
    case 1:                                                                                                                   //Batterie schwach
        smtpData.setSubject("Bewässerungsanlage - Batterie schwach");                                                         // Set the subject
        smtpData.setMessage("<div style=\"color:#2f4468;\"><h1>Hello World!</h1><p>- Sent from ESP32 board</p></div>", true); // Set the message with HTML format
        // Set the email message in text format (raw)
        //smtpData.setMessage("Hello World! - Sent from ESP32 board", false);
        break;
    case 2:                                                                                                                   //Wassertank leer
        smtpData.setSubject("Bewässerungsanlage - Wassertank leer");                                                          // Set the subject
        smtpData.setMessage("<div style=\"color:#2f4468;\"><h1>Hello World!</h1><p>- Sent from ESP32 board</p></div>", true); // Set the message with HTML format
        // Set the email message in text format (raw)
        //smtpData.setMessage("Hello World! - Sent from ESP32 board", false);
        break;
    default: //default
        break;
    }

    smtpData.addRecipient(emailRecipient); // Add recipients, you can add more than one recipient
    smtpData.setSendCallback(sendCallback);

    //Start sending Email, can be set callback function to track the status
    if (!MailClient.sendMail(smtpData))
    {
        Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    }
    smtpData.empty(); //Clear all data from Email object to free memory
}

void pump_actv(bool acvn)
{
    if (acvn == true)
    {
        Serial.println("Pumpe wird angesteuert");
        digitalWrite(PIN_PUMP, HIGH);
    }
    else
    {
        Serial.println("Pumpe  wird nicht mehr angesteuert");
        digitalWrite(PIN_PUMP, LOW);
    }
}

//Setup Routine
/***************************************************************************
 * Aufgrund Deep-Sleep muss die gesamte Funktionalität in der Setup Routine erfolgen
 * in: kein
 * out: kein
 **************************************************************************/
void setup()
{
    //Initialisieren von Variablen
    int32_t duration = 0;
    uint16_t water_ammount = 0;
    uint16_t water_ammount_thd = 40;
    uint8_t soil_hum = 0;
    uint8_t soil_hum_thd = 40; //Schwelle für Bodenfeuchtigkeit, ab der gegossen werden soll
    uint16_t pump_counter = 0; //Zähler für Pumpdauer
    uint16_t pump_dur = 50;    //Dauer, wie lange die Pumpe angesteuert werden soll
    uint8_t battery_status = 0;
    uint8_t battery_status_thd = 40;

    //Initialisiere Seriellen Monitor
    Serial.begin(115200);
    delay(1000);

    //Auslesen der Anzahl der reboots
    bootCount++;
    Serial.println("Boot number: " + String(bootCount));

    //Ausgabe des Wakeup-Grunds
    print_wakeup_reason();

    //Initialisiere DeepSleep mit zyklischem Wakeup alle x Minuten
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //deep-sleep Zeit in Mikrosekunden
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " seconds");

    //Bodenfeuchtigkeitssensor
    pinMode(PIN_SOILSENS, INPUT);

    //Ultraschallsensor
    pinMode(PIN_TRIGGER, OUTPUT); // Sets the trigPin as an Output
    pinMode(PIN_ECHO, INPUT);     // Sets the echoPin as an Input

    //Pumpe
    pinMode(PIN_PUMP, OUTPUT);
    digitalWrite(PIN_PUMP, false); //Pumpe ausschalten

    //Batteriemonitoring
    //TODO: Testzwecke fester Batteriezustand
    battery_status = 50;
    if (battery_status <= battery_status_thd) //Bewässerungsanalge nicht nutzbar
    {
        //Batterie wird schwach --> Mail an hinterlegten Empfänger
        send_mail(1);
    }
    else //Bewässerungsanalge nutzbar
    {
        //Vorbereitung Messung
        digitalWrite(PIN_TRIGGER, LOW); // Löschen Trigger-Pin
        delayMicroseconds(2);

        //Messvorgang starten
        digitalWrite(PIN_TRIGGER, HIGH); // PIN_TRIGGER für 10 Mikrosekunden auf HIGH setzen
        delayMicroseconds(10);
        digitalWrite(PIN_TRIGGER, LOW);
        duration = pulseIn(PIN_ECHO, HIGH); //Auswertung Dauer Rückantwort

        //Berechnung Wasserstand
        water_ammount = duration * 0.034 / 2;
        water_ammount = 50; //TODO: Testzwecke fester Wasserstand
        Serial.print("Wasserstand: ");
        Serial.println(water_ammount);
        if (water_ammount >= water_ammount_thd)
        {
            //Wasserstand iO --> Bewässerungsanlage kann genutzt werden

            //Messe Bodenfeuchtigkeit
            soil_hum = analogRead(PIN_SOILSENS);
            soil_hum = 50; //TODO: Testzwecke feste Bodenfeuchtigkeit
            Serial.print("Bodenfeuchtigkeit: ");
            Serial.println(soil_hum);
            if (soil_hum <= soil_hum_thd)
            {
                //Boden zu trocken, daher muss Pumpe aktiviert werden
                while (pump_counter < pump_dur)
                {
                    //Pumpe läuft kürzer als Ansteuerzeit --> weiter ansteuern
                    pump_actv(true);
                }
                //Pumpdauer erreicht --> Pumpe abschalten
                pump_actv(false);
            }
        }
        else
        {
            //Bewässerungsanlage kann nicht genutzt werden --> Mail an hinterlegten Empfänger
            send_mail(2);
        }
    }

    //ESP32 in den Deep-Sleep versetzen
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start();
    //nach dieser Zeile, keine Code Zeile mehr kommen, diese wird nicht mehr ausgeführt
}

//Setup Routine
/***************************************************************************
 * Aufgrund Deep-Sleep wird dieser Umfang nie aktiviert
 * in: kein
 * out: kein
 **************************************************************************/
void loop()
{
    //wird aufgrund Deep Sleep nicht erreicht.
    //Volle Funktionalität ist in setup-Routine enthalten
}
