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
String timeStamp;

TaskHandle_t Task1;
TaskHandle_t Task2;

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

// Task1code: blinks an LED every 1000 ms
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
                // for (int i = 90; i > 0; i-=1) {
                //   myServo.write(i);
                //   delay(20);
                // }
                pos1 = header.indexOf('=');
                pos2 = header.indexOf('&');
                valueString = header.substring(pos1 + 1, pos2);

                if (valueString.toInt())
                {
                  Serial.println("Feeding...");
                  myServo.write(0);
                  delay(1000);
                  myServo.write(90);
                  Serial.println("Done!");
                }
              }
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

// Task2code: blinks an LED every 700 ms
void Task2code(void *pvParameters)
{
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    timeClient.update();
    timeStamp = String(timeClient.getEpochTime());
    Serial.println(timeStamp);
    delay(1000);
    // Serial.println(timeClient.getFormattedTime());
  }
}

void loop()
{
}

String SendHTML()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head>\n";
  ptr += "<title>Fish Feeder</title>\n";
  ptr += "<style>\n";
  ptr += "body{margin-top: 5rem;text-align: center;}\n";
  ptr += "#feed {display:block;background-color: #3498db;border:none;color:white;padding:1rem;font-size:2rem;margin-left:auto;margin-right:auto;border-radius:5%;}\n";
  ptr += "p {color:red;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Fish Feeder</h1>\n";
  ptr += "<p id=\"helper-text\"></p>";
  ptr += "<button id=\"feed\" onclick=\"feed()\">Feed</button>\n";

  ptr += "<script>\n";
  ptr += "var button = document.getElementById(\"feed\");\n";
  ptr += "var helper = document.getElementById(\"helper-text\");\n";
  ptr += "async function feed() {\n";
  ptr += "try { helper.innerHTML = ''; button.innerHTML = \"feeding\"; button.style.backgroundColor = \"grey\"; button.disabled = true; const res = await fetch(\"/?value=1&\");} \n";
  ptr += "catch (err) { console.error(\"Operation failed: \", err); helper.innerHTML = `Operation failed: ${err}`; }";
  ptr += "finally { setTimeout(() => { button.innerHTML = \"feed\"; button.disabled = false; button.style.backgroundColor = \"#3498db\"; }, \"1000\"); }}";
  ptr += "</script>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}