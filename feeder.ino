#include <ESP32Servo.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

static const int servoPin = 13;
Servo myServo;

const char *ssid = "36";
const char *password = "okemos36";

WiFiServer server(80);

WiFiUDP ntpUDP; // NTP client for getting time
NTPClient timeClient(ntpUDP);

String header;
String valueString = String(5);
int pos1 = 0;
int pos2 = 0;
unsigned long timeStamp;

TaskHandle_t Task1;
TaskHandle_t Task2;

int setTime = 55080; // = hr * 3600 + min * 60

void connect_to_Wifi()
{
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  timeClient.begin();               // initialize time server
  timeClient.setTimeOffset(-14400); // offset timezone
}

void feed()
{
  Serial.println("Feeding...");
  myServo.write(0);
  delay(1000);
  myServo.write(90);
  Serial.println("Done!");
}

void setup()
{
  Serial.begin(115200);
  connect_to_Wifi();
  myServo.attach(servoPin);

  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
      Task1code, /* Task function. */
      "Task1",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */
  delay(500);

  // create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
      Task2code, /* Task function. */
      "Task2",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task2,    /* Task handle to keep track of created task */
      1);        /* pin task to core 1 */
  delay(500);
}

void Task1code(void *pvParameters)
{
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    WiFiClient client = server.available();

    if (client)
    {
      Serial.println("Client connected");
      String currentLine = "";
      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          header += c;
          if (c == '\n')
          {
            if (currentLine.length() == 0)
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              if (header.indexOf("GET /?value=") >= 0)
              {
                pos1 = header.indexOf('=');
                pos2 = header.indexOf('&');
                valueString = header.substring(pos1 + 1, pos2);

                if (valueString.toInt())
                {
                  feed();
                }
              } else if (header.indexOf("GET /?time=") >= 0)
              {
                pos1 = header.indexOf('=');
                pos2 = header.indexOf('&');
                setTime = header.substring(pos1 + 1, pos2).toInt();
              }
              Serial.println("Time set: " + String(setTime));
              client.println(SendHTML());
              client.println();
              break;
            }
            else
            {
              currentLine = "";
            }
          }
          else if (c != '\r')
          {
            currentLine += c;
          }
        }
      }
      Serial.println("Client disconnected");
      header = "";
    }
    delay(1000);
  }
}

void Task2code(void *pvParameters)
{
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    timeClient.update();
    timeStamp = timeClient.getEpochTime();
    int mod = (timeStamp - setTime) % 86400;
    if (mod == 0) {
      Serial.println(timeClient.getFormattedTime());
      feed();
    }
    delay(1000);
  }
}

void loop()
{
}

String SendHTML()
{
  String hour;
  String minute;
  int hr = setTime / 3600;
  if (hr < 10) {
    hour = "0" + String(hr);
  } else {
    hour = String(hr);
  }
  
  int min = (setTime - hr * 3600) / 60;
  if (min < 10) {
    minute = "0" + String(min);
  } else {
    minute = String(min);
  }
  Serial.println(hour + ":" + minute);

  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head>\n";
  ptr += "<title>Fish Feeder</title>\n";
  ptr += "<style>\n";
  ptr += "body{margin-top: 5rem;text-align: center;}\n";
  ptr += "#feed {display:block;background-color: #3498db;border:none;color:white;padding:1rem;font-size:2rem;margin: 1rem auto 1rem auto;border-radius:5%;}\n";
  ptr += "p {color:red;}\n";
  ptr += "";
  ptr += "label { display: block; font: 1rem 'Fira Sans', sans-serif; }";
  ptr += "input, label { margin: 0.4rem 0; }";

  ptr += "</style>\n";
  ptr += "</head>\n";

  ptr += "<body>\n";
  ptr += "<h1>Fish Feeder</h1>\n";
  ptr += "<p id=\"helper-text\"></p>";
  ptr += "<button id=\"feed\" onclick=\"feed()\">Feed</button>\n";
  ptr += "<label for=\"time\">Choose a time for automatic feeding:</label>";
  ptr += "<input type=\"time\" id=\"time\" name=\"time\" min=\"00:00\" max=\"24:00\" onchange=\"changeTime()\" ";
  ptr += "value='" + hour + ":" + minute + "' required>\n";

  ptr += "<script>\n";
  ptr += "const button = document.getElementById(\"feed\");\n";
  ptr += "const helper = document.getElementById(\"helper-text\");\n";
  ptr += "const time = document.getElementById(\"time\");\n";

  ptr += "const feed = async () => {\n";
  ptr += "try { helper.innerHTML = ''; button.innerHTML = \"feeding\"; button.style.backgroundColor = \"grey\"; button.disabled = true; const res = await fetch(\"/?value=1&\");} \n";
  ptr += "catch (err) { console.error(\"Operation failed: \", err); helper.innerHTML = `Operation failed: ${err}`; }";
  ptr += "finally { setTimeout(() => { button.innerHTML = \"feed\"; button.disabled = false; button.style.backgroundColor = \"#3498db\"; }, \"1000\"); }}\n";
  
  ptr += "const changeTime = async () => {\n";
  ptr += "const val = time.value.split(':'); const hour = parseInt(val[0]); const min = parseInt(val[1]); const newTime = hour * 3600 + min * 60;\n";
  ptr += "try { helper.innerHTML = ''; time.disabled = true; const res = await fetch(`/?time=${newTime}&`); }\n";
  ptr += "catch (err) { console.error(\"Failed to set time: \", err); helper.innerHTML = `Failed to set time: ${err}`; }";
  ptr += "finally { time.disabled = false; } }";
  
  ptr += "</script>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}