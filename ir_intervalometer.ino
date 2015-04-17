#include <IRremote.h>

// Setup some states for the program.
#define RECEIVE     0
#define IMAGE_START 1
#define IMAGE_TIME  2
#define IMAGE_END   3
#define IMAGE_ABORT 4
#define WAIT        5
#define RESET       6

const static char stateNames[][20] = {{"Receive"},
                                      {"Image Start"},
                                      {"Image Time"},
                                      {"Image End"},
                                      {"Image Abort"},
                                      {"Wait"},
                                      {"Reset"}};

// Debug enable or disable */
#define debug_println(x)   //Serial.println(x)
#define debug_print_hex(x) //Serial.print(x,HEX)
#define debug_print(x)     //Serial.print(x)

#define IR_SHUTTER  0xB4B8F
#define IR_INTERVAL 40

#define WAIT_DURATION   10000
#define IMAGE_DURATION  300000

#define USER_LED 13

// Setup the receive, gnd, pwr pins.
#define RECV_PIN     11
#define RECV_GND_PIN 10
#define RECV_PWR_PIN 9

// Setup the send, gnd pins.
#define SEND_GND_PIN 5
#define SEND_PIN     3 /* Assumed by the library. */

// Setup a send and a receive object.
IRrecv irrecv(RECV_PIN);
IRsend irsend;

// Setup a couple of variables.
unsigned long sendValue;
unsigned int state;
unsigned int imageCount;
unsigned long imageDuration;
unsigned long transitionTime;

unsigned long setDuration;
unsigned int setImages;

void sendMessage(unsigned long value, int length)
{
  irsend.sendSony(value, length);
  delay(IR_INTERVAL);
  irsend.sendSony(value, length);
  delay(IR_INTERVAL);
  irsend.sendSony(value, length);
  delay(IR_INTERVAL);

  irrecv.enableIRIn();  
  
}

void nextState(int newState)
{
  transitionTime = millis();
  state = newState;
  
  Serial.print("Transition to -> " );
  Serial.println(stateNames[newState]);
  
  if (newState == IMAGE_TIME)
  {
    imageCount++;
    digitalWrite(USER_LED, HIGH);
  }
  
  if (newState == IMAGE_END || newState == IMAGE_ABORT)
  {
    digitalWrite(USER_LED, LOW);
  }
}


// The standard Arduino setup program. This starts the
// serial interface as well as the IR input interface.
// The state is initialized to Receive and pin 13 is 
// setup as an output pin. This is tied to the onboard
// LED and will be used for program status indication.

void setup()
{ 
  pinMode(RECV_GND_PIN,OUTPUT);
  pinMode(RECV_PWR_PIN,OUTPUT);
  pinMode(SEND_GND_PIN,OUTPUT);
  
  digitalWrite(RECV_GND_PIN,0);
  digitalWrite(RECV_PWR_PIN,1);
  digitalWrite(SEND_GND_PIN,0);
  
  Serial.begin(9600);
  
  irrecv.enableIRIn(); // Start the receiver  
  
  state = RECEIVE;
  imageCount = 0;

  setDuration = 60;
  setImages = 1;

  pinMode(USER_LED, OUTPUT);
  
  Serial.println("System Init Complete");
}

// The standard Arduino loop program. This processes the
// states of the program.
void loop()
{
  unsigned long currentTime = millis();
  
  switch (state)
  {
    // The RECEIVE state waits for an IR message, if a
    // valid one is found it increments the payload and
    // stores it in the sendValue variable.
    // A valid message is one that has been decoded as
    // a Sony message.
    case RECEIVE:

      decode_results results;
    
      if (irrecv.decode(&results)) 
      {
        debug_println("--- NEW CYCLE ---");
        debug_print_hex(results.value);
        debug_print(", ");
        debug_print_hex(results.decode_type);
        debug_print(", ");
        debug_println(results.bits);
      
        if (results.decode_type == SONY)
        {
          sendValue = (results.value);
          nextState(IMAGE_TIME);
        }
        irrecv.resume();
      }
      break;

    case WAIT:
    
      if (transitionTime + WAIT_DURATION < currentTime)
      {
        nextState(IMAGE_START);
        
        debug_print("Wait - ");
        debug_print(WAIT_DURATION);
        debug_println("ms");
      }
      
      break;

    /* IMAGE state:
     * This is the state that starts the image. The shutter opens in this state. 
     */
    case IMAGE_START:

      sendMessage(sendValue, 20);

      if (imageCount < setImages)
      {
        /* Set the correct millisecond delay from the set duration. */
        imageDuration = setDuration * 1000;
        nextState(IMAGE_TIME);
      }
      else
      {
        nextState(RESET);
      }
      
      debug_print("Image Start - ");
      debug_print_hex(sendValue);
      debug_println();
      
      break;
    
    /* IMAGE_TIME state:
     * This is the state that times the image. The shutter is open during this state.
     */
    case IMAGE_TIME:

      if (transitionTime + imageDuration < currentTime)
      {
        nextState(IMAGE_END);
        debug_println("Image Time"); 
      }
      
      if (((currentTime - transitionTime) % 30000) == 0)
      {
        debug_print(imageCount);
        debug_print(" : ");
        debug_print(imageDuration);
        debug_print(" : ");
        debug_println(currentTime - transitionTime);
      }
      
      break;

    case IMAGE_END:
      
      sendMessage(sendValue, 20);
      nextState(WAIT);
      
      debug_print("Image End - ");
      debug_print_hex(sendValue);
      debug_println();
      
      break;

    case IMAGE_ABORT:
      
      sendMessage(sendValue, 20);
      nextState(RESET);
      
      debug_print("Image Abort - ");
      debug_print_hex(sendValue);
      debug_println();
      
      break;
    
    case RESET:
    {
      imageCount = 0;
    }
  }
}

int string2int(char * string, int length)
{
  int value = 0;
  int offset = 1;
  
  while (length > 0)
  {
    length--;
    value = (string[length] - '0') * offset + value;
    offset*=10;
  }
  return value;
}

/* This will look for a string over serial.
 * The possible strings are:
 *   iXXsXXXXX
 *   start
 */
void serialEvent()
{
  char readValue[10] = {0,0,0,0,0,0,0,0,0,0};
  int readLength;
  
  if (Serial.available() > 0)
  {
    if (Serial.peek() == 'i')
    {
      Serial.read();
      readLength = Serial.readBytesUntil('s', readValue, 5);
      setImages = string2int(readValue, readLength);
      readLength = Serial.readBytesUntil('x', readValue, 5);
      setDuration = string2int(readValue, readLength);
      
      Serial.println();
      Serial.print("r");
      Serial.print(setImages);
      Serial.print("d");
      Serial.print(setDuration);
      Serial.println("x");
      
      Serial.readBytes(readValue,Serial.available());
    }
    else if (Serial.peek() == 's')
    {
      Serial.println();
      Serial.print("s");
      Serial.print(setImages);
      Serial.print("d");
      Serial.print(setDuration);
      Serial.println("x");
      sendValue = IR_SHUTTER;
      nextState(IMAGE_START);
      Serial.readBytes(readValue,Serial.available());
    }
    else if (Serial.peek() == 'e')
    {
      Serial.println();
      Serial.print("e");
      Serial.print(setImages);
      Serial.print("d");
      Serial.print(setDuration);
      Serial.println("x");
      sendValue = IR_SHUTTER;
      nextState(IMAGE_ABORT);
      Serial.readBytes(readValue,Serial.available());
    }
    else
    {
      debug_print("Clearing Bytes - ");
      debug_println(Serial.available());
      Serial.readBytes(readValue,Serial.available());
    }
  }
}

