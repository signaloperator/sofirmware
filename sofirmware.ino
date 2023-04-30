/*
Signal Operator / Arduino Uno R3 / Protocol 0.1.0.0 / Firmware 0.1.0.0

( License : https://www.gnu.org/licenses/gpl-3.0.en.html )
( Tutorial: https://github.com/signaloperator/sotutorial )
( Protocol: https://github.com/signaloperator/soprotocol )
*/

#include <Wire.h> /* Use I2C library */
#include <SPI.h>  /* Use SPI library */

#define INCOMING_COMMAND_SIZE       256  /* Buffer size of incoming command */
#define INCOMING_COMMAND_MS         5    /* After N milliseconds of no incoming bytes, command is considered completed. */
#define INCOMING_COMMAND_PAUSE_MS   500  /* If the current incoming command is too long, don't process incoming bytes for N milliseconds. */
#define INCOMING_COMMAND_PART_SIZE  16   /* Buffer size of one part of incoming command */
#define SPI_DELAY_US                3    /* Delay of N microseconds for SPI */
#define SPI_SPEED                   ((uint32_t)1000000)  /* SPI speed */

/* Text to display on start */
const char welcomeText[] = "\r\n\r\nSignal Operator / Arduino Uno R3\r\nProtocol 0.1.0.0 / Firmware 0.1.0.0\r\n(Use the \"help\" command for tips of usage)\r\n";
/* Text to display for the "help" command */
const char helpText[] = ">License : https://www.gnu.org/licenses/gpl-3.0.en.html\r\n>Tutorial: https://github.com/signaloperator/sotutorial\r\n>Protocol: https://github.com/signaloperator/soprotocol\r\n"; 

uint8_t incomingByte = 0;  /* The current incoming byte */
unsigned long incomingByteMs = 0; /* Timestamp(milliseconds) of the last incoming byte */
char incomingCommand[INCOMING_COMMAND_SIZE] = { 0 }; /* Buffer of incoming command */
uint16_t incomingCommandIndex = 0; /* Current index of incoming command buffer, where the next incoming byte will be saved. */
uint8_t gotFirstLetter = 0; /* Whether we received the first byte that's not space */
uint8_t previousByteIsSpace = 0; /* Whether the previous byte is space */

/* Initialize */
void setup() {
  /* Start the I2C bus as a controller */
  Wire.begin();
  /* Start the SPI bus */
  SPI.begin();
  /* Default baud rate */
  Serial.begin(9600);
  /* Wait for serial port */
  while(!Serial){}
  /* Send the welcome text */
  Serial.print(welcomeText);
  /* Print "#" (wait for input) */
  Serial.print("#");             
}

/* Convert string to lower case */
char *stringToLower(char *s) {
  /* Loop to the end 0x00 */
  for(char *p=s; *p; p++) {
    /* Convert this character to lower case */
    *p=tolower(*p);
  }
  /* Return the string */
  return s;
}

/* Check whether str1 begins with str2 */
uint8_t checkStringBeginning(char *str1, char *str2) {
  /* Length of str1 */
  uint16_t length1 = strlen(str1);
  /* Length of str2 */
  uint16_t length2 = strlen(str2);
  /* if str2 is longer than str1 */
  if(length2>length1) {
    /* Return false */
    return 0;
  }
  /* Variable for loop */
  uint16_t i = 0;
  /* Loop through str2 */
  for(i=0;i<length2;i++) {
    /* If the current character is not equal */    
    if(str1[i]!=str2[i]) {
      /* Return false */
      return 0;
    }
  }
  /* Return true */
  return 1;    
}

/* Check whether str1 is string of the str2 command */
uint8_t checkCommandType(char *str1, char *str2) {
  /* Check whether str1 begins with str2 */
  if(checkStringBeginning(str1, str2)) {
    /* Get the next character */
    char nextChar = str1[strlen(str2)];
    /* It should be space or 0x00 */
    if((nextChar==' ')||(nextChar==0x00)) {
      /* Yes */
      return 1;
    }
  }
  /* Otherwise - Nope */
  return 0;
}

/* Print HEX of byte, and then a space character*/
void printByte(uint8_t b) {
  /* If it's one digit HEX */
  if (b<0x10) {
    /* Print zero */
    Serial.print("0");
  }
  /* Print byte */
  Serial.print(b, HEX);
  /* And a space character */
  Serial.print(" ");
}

/* Get the number of parts in command 
   str should be in the format of "a b c d", 
   so we just count the number space characters, and then add one. */
uint16_t getNumberOfParts(char *str) {
  /* n: how many parts, i: loop, l: length of string */
  uint16_t n = 0, i = 0, l = 0;
  /* Get the length of string */
  l = strlen(str);
  /* Loop through every character */
  for(i=0;i<l;i++) {
    /* If it's space character */
    if(str[i]==' ') {
      /* Count */
      n++;
    }
  }
  /* In the end, add one */
  n++;
  /* Return the result */
  return n;  
}

/* Get a specific part of command, by its index
   (index of the first part is 0) 
   This returns 0 on success, and 1 on failure */
uint8_t getSinglePart(char *str, char *index, char *output, uint16_t output_buffer_size) {
  /* n: the current index, i: loop, l: length of string */
  uint16_t n = 0, i = 0, l = 0;
  /* The start of requested part, and the end of requested part */
  char *pStart = NULL, *pEnd = NULL;  
  /* Get the length of string */
  l = strlen(str);
  /* Loop through every character */
  for(i=0;i<l;i++) {
    /* If it's space character */
    if(str[i]==' ') {
      /* Count */
      n++;
      /* Skip the rest of process */
      continue;
    }
    /* If the current index is the requested index */
    if((n==index)&&(pStart==NULL)) {
      /* The requested part starts here */
      pStart =  str+i;     
    }
    /* If the current index is the requested index plus 1 */
    if((n==(index+1))&&(pEnd==NULL)) {
      /* The end of requested part. "-1-1" for space char, and the letter after space char */
      pEnd = str+i-1-1;
    }
  }
  /* If we never found the start (of requested part) */
  if(pStart==NULL) {
    /* Set output as empty string */
    output[0] = 0x00;
    /* Return an error code */
    return 1;
  }
  /* If we never found the end (of requested part) */ 
  if(pEnd==NULL) {
    /* It ends at the last char */
    pEnd = str+l-1;
  }
  /* If the output buffer is too small 
     "pEnd+1-pStart" is the number of characters. 
     (If pEnd = pStart, it's 1 character) 
     It needs one more character of the ending 0x00 */
  if(output_buffer_size<(pEnd+1-pStart+1)) {
    /* Return an error code */
    return 2;    
  }
  /* Put zeros into output */
  memset(output, 0x00, output_buffer_size);
  /* Now copy the result 
     "pEnd+1-pStart" is the number of characters. 
     (If pEnd = pStart, it's 1 character) */
  strncpy(output, pStart, pEnd+1-pStart);
  /* And return an OK code */
  return 0;
}

/* Get byte from hex string */
uint8_t getByteFromHex(char *s, uint8_t *x) {
  /* Char variable */
  char c = 0x00;
  /* Result variable */
  uint8_t result = 0;
  /* Get the first char */
  c = *s;
  /* If it's 0 to 9 */
  if((c>='0')&&(c<='9')) {
    /* Put it into result */
    result = (c - '0')*0x10;
  }
  /* If it's a to f */
  else if((c>='a')&&(c<='f')) {
    /* Put it into result */
    result = (c - 'a' + 0xa)*0x10;
  }
  /* If it's a wrong char */
  else {
    /* Return an error code */
    return 1;
  }
  /* Get the second character */
  c = *(s+1);
  /* If it's 0 to 9 */
  if((c>='0')&&(c<='9')) {
    /* Add it into result */
    result += c - '0';
  }
  /* If it's a to f */
  else if((c>='a')&&(c<='f')) {
    /* Add it into result */
    result += c - 'a' + 0xa;
  }
  /* If it's a wrong char */
  else {
    /* Return an error code */
    return 2;
  }
  /* Output the result */
  *x = result;
  /* Return an OK code */
  return 0;
}

/* Execute the incoming command */
void executeIncomingCommand(void) {
  /* If it's empty string */
  if(incomingCommand[0]==0x00) {
    /* Say the error message */
    Serial.print(">Detail: Empty command.\r\n>Status: ERR\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* If the last character is space */
  if(incomingCommand[strlen(incomingCommand)-1]==' ') {
    /* Set it as 0x00 */
    incomingCommand[strlen(incomingCommand)-1]=0x00;
  }
  /* Check again: If it's empty string */
  if(incomingCommand[0]==0x00) {
    /* Say the error message */
    Serial.print(">Detail: Empty command.\r\n>Status: ERR\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* convert string to lower case */
  stringToLower(incomingCommand);
  /* The "help" command - show tips*/
  if(checkCommandType(incomingCommand, "help")) {
    /* Show help */
    Serial.print(helpText);
    /* Cancel the remaining process */
    return;
  }
  /* The "i2cscan" command - scan I2C addresses*/
  if(checkCommandType(incomingCommand, "i2cscan")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Variable for loop */        
    uint8_t i = 0;
    /* Variable for error code */
    uint8_t error = 0;
    /* Loop through all possible I2C addresses 
       Binary 0111 1111  = 127 (DEC) = 0x7F (HEX) 
       (Some numbers in this loop are invalid, but we go through them anyway) */
    for(i=0;i<=0x7F;i++) {
      /* Start I2C operation */
      Wire.beginTransmission(i);
      /* End I2C operation 
         ("true" will send a stop message, releasing the bus after transmission.)*/
      error = Wire.endTransmission(true);
      /* 0: success */
      if(error==0) {
        /* Print this address */
        printByte(i);        
      }
    }
    /* Print the new-line characters */
    Serial.print("\r\n");
    /* Show the "OK" status */
    Serial.print(">Status: OK\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* "testparts" command, test the parsing of command */
  if(checkCommandType(incomingCommand, "testparts")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* Print the number of parts */
    Serial.print(numberOfParts);
    /* Print extra text */
    Serial.print("(DEC) - ");
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* Variable for loop */
    uint16_t i = 0;
    /* Print every part of the command */
    for(i=0;i<numberOfParts;i++) {
      /* If we got the part */
      if(getSinglePart(incomingCommand, i, part, INCOMING_COMMAND_PART_SIZE)==0) {
        /* Print "[part] " */
        Serial.print("["); Serial.print(part); Serial.print("] ");
      }
      /* Failed to get this part */
      else {
        /* Print an error message */
        Serial.print("[***INVALID***] ");
      }
    }
    /* Print the new-line characters */
    Serial.print("\r\n");
    /* Show the "OK" status */
    Serial.print(">Status: OK\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* "i2cwrite" command: write data to I2C bus 
     Format: i2cwrite @50 11 22 33 nostop */
  if(checkCommandType(incomingCommand, "i2cwrite")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If there are too few parameters */
    if(numberOfParts<3) {
      /* Complain about too few parameters */
      Serial.print("Too few parameters.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* The current byte data */
    uint8_t currentByteData = 0;
    /* Get the second part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* If the second part is not "@AB" (AB is the I2C addresss)
       (Length is not 3, or the first char is not '@', or the remaining two chars are not hex) */
    if((strlen(part)!=3)||(part[0]!='@')||(getByteFromHex(part+1, &currentByteData)!=0)) {
      /* Complain about too few parameters */
      Serial.print("Invalid I2C address.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Variable for loop */
    uint16_t i = 0;
    /* Loop through the parts of data to write (we have already checked the number of parts) */
    for(i=2;i<=numberOfParts-1-1;i++) {
      /* Get the current part */
      getSinglePart(incomingCommand, i, part, INCOMING_COMMAND_PART_SIZE);
      /* If it's not hex of byte */
      if(getByteFromHex(part, &currentByteData)!=0) {
        /* Complain about invalid I2C data */
        Serial.print("Invalid I2C data.\r\n");
        /* Show the "ERR" status */
        Serial.print(">Status: ERR\r\n");
        /* Cancel the remaining process */
        return;
      }
    }
    /* Get the last part */
    getSinglePart(incomingCommand, numberOfParts-1, part, INCOMING_COMMAND_PART_SIZE);
    /* If it's neither "stop" nor "nostop" */
    if((strcmp(part, "stop")!=0)&&(strcmp(part, "nostop")!=0)) {
        /* Complain about invalid ending of command */
        Serial.print("The last part is neither \"stop\" nor \"nostop\".\r\n");
        /* Show the "ERR" status */
        Serial.print(">Status: ERR\r\n");
        /* Cancel the remaining process */
        return;
    }
    /* Get the second part */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* Get the I2C addresss) */
    getByteFromHex(part+1, &currentByteData);
    /* Start I2C transaction */
    Wire.beginTransmission(currentByteData);
    /* Write data to I2C bus */
    /* Loop through the parts of data to write (we have already checked the number of parts) */
    for(i=2;i<=numberOfParts-1-1;i++) {
      /* Get the current part */
      getSinglePart(incomingCommand, i, part, INCOMING_COMMAND_PART_SIZE);
      /* Convert it to byte */        
      getByteFromHex(part, &currentByteData);
      /* Write this byte to I2C */
      Wire.write(currentByteData);
    }
    /* I2C result */
    uint8_t result = 0;
    /* Get the last part */
    getSinglePart(incomingCommand, numberOfParts-1, part, INCOMING_COMMAND_PART_SIZE);
    /* If it's "stop" */
    if(strcmp(part, "stop")==0) {
      /* Get result */
      result = Wire.endTransmission(true);
    }
    else {
      /* Get result */
      result = Wire.endTransmission(false);
    }
    /* If it's successful */
    if(result==0) {
      /* Say it's successful */
      Serial.print("\"i2cwrite\" is successful.\r\n");
      /* Show the "OK" status */
      Serial.print(">Status: OK\r\n");
    }
    /* Not successful */
    else {
      /* Say it failed */
      Serial.print("\"i2cwrite\" failed.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");      
    }
    /* Cancel the remaining process */
    return;
  }
  /* "testhex" command, test the parsing of hex */
  if(checkCommandType(incomingCommand, "testhex")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";    
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If there are two parts */
    if(numberOfParts==2) {
      /* The byte data */
      uint8_t byteData = 0;
      /* Get the 2nd part */
      getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);
      /* Status code of the hex-to-byte conversion */
      uint8_t convertStatus = getByteFromHex(part, &byteData);
      /* If the conversion failed */
      if(convertStatus!=0) {
        /* Say there is error */
        Serial.print("There is error, Code = ");
        /* Print the error code */
        Serial.print(convertStatus);
        /* Print the ERR status */
        Serial.print("\r\n>Status: ERR\r\n");
        /* Cancel the remaining process */
        return;
      }
      /* Print the byte */
      printByte(byteData);
      /* Say it's OK */
      Serial.print("\r\n>Status: OK\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Wrong number of parts */
    else {
      Serial.print("Wrong number of parts.\r\n>Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
  }
  /* "i2cread" command: read data from I2C bus 
     Format: i2cread @50 AB stop
     (50 is the I2C address, AB is the length of data to read, and stop/nostop at the end) */
  if(checkCommandType(incomingCommand, "i2cread")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If the number of parameters is wrong */
    if(numberOfParts!=4) {
      /* Complain about the number of parameters */
      Serial.print("The number of parameters is wrong.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* The current byte data */
    uint8_t currentByteData = 0;
    /* Get the second part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* If the second part is not "@AB" (AB is the I2C addresss)
       (Length is not 3, or the first char is not '@', or the remaining two chars are not hex) */
    if((strlen(part)!=3)||(part[0]!='@')||(getByteFromHex(part+1, &currentByteData)!=0)) {
      /* Complain about too few parameters */
      Serial.print("Invalid I2C address.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Save the I2C address */
    uint8_t i2cAddress = currentByteData;
    /* The the length part */
    getSinglePart(incomingCommand, 2, part, INCOMING_COMMAND_PART_SIZE);
    /* If it's not valid hex-byte number */
    if(getByteFromHex(part, &currentByteData)!=0) {
      /* Complain about invalid I2C data */
      Serial.print("Invalid length to read from I2C.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Save the length of reading */
    uint8_t i2cLengthOfReading = currentByteData;
    /* Get the last part */
    getSinglePart(incomingCommand, 3, part, INCOMING_COMMAND_PART_SIZE);
    /* If it's neither "stop" nor "nostop" */
    if((strcmp(part, "stop")!=0)&&(strcmp(part, "nostop")!=0)) {
        /* Complain about invalid ending of command */
        Serial.print("The last part is neither \"stop\" nor \"nostop\".\r\n");
        /* Show the "ERR" status */
        Serial.print(">Status: ERR\r\n");
        /* Cancel the remaining process */
        return;
    }
    /* Whether send stop at the end */
    uint8_t i2cStop = 0;
    /* If it's "stop" */
    if(strcmp(part, "stop")==0) {
      /* Yes, stop at the end */
      i2cStop = 1;
    }
    /* Otherwise */
    else {
      /* No stop at the end */
      i2cStop = 0;
    }    
    /* I2C result */
    uint8_t result = 0;
    /* Reading from I2C 

       Note #1: 
       "requestFrom" is defined as 
       uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
       which calls:
       uint8_t read = twi_readFrom(address, rxBuffer, quantity, sendStop);
       which executes:
       twi_sendStop = sendStop;
       which is used as:
       if (twi_sendStop)
       
       Note #2:
       Official document says "Wire.requestFrom(address, quantity, stop)", and "stop: true or false"
       It's inaccurate because "stop" is defined as "uint8_t".

       Note #3:
       There is a more powerful function
       uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop)
       But this one is not used, because: 
       It's not in official documents, and it does
       "beginTransmission(address); ... write( ... iaddress ... ); ... endTransmission(false);"
       Our "i2cwrite" command has this stuff.

       Note #4:
       Other Arduino variants might have "bool stopBit". In this case, change the following code to:
       result = Wire.requestFrom(i2cAddress, i2cLengthOfReading, (i2cStop?true:false));
       (It's not tested, as we mainly focus on Arduino Uno R3)
       */
    result = Wire.requestFrom(i2cAddress, i2cLengthOfReading, i2cStop);    
    /* If it's successful */
    if(result==i2cLengthOfReading) {
      /* Print the received bytes */
      while (Wire.available()) {
        /* Get one byte */
        currentByteData = Wire.read();
        /* Print this byte */
        printByte(currentByteData);
      }
      /* Print new line */
      Serial.print("\r\n");
      /* Show the "OK" status */
      Serial.print(">Status: OK\r\n");
    }
    /* Not successful */
    else {
      /* Empty the buffer by reading all (even though it failed) */
      while (Wire.available()) {
        /* Get one byte */
        currentByteData = Wire.read();
      }
      /* Say it failed */
      Serial.print("\"i2cread\" failed - expect ");
      /* Print the number of bytes to read */
      printByte(i2cLengthOfReading);
      /* Print some text */
      Serial.print("bytes, got ");
      /* Print the number of bytes we got */
      printByte(result);
      /* Print some text */
      Serial.print("bytes.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");      
    }
    /* Cancel the remaining process */
    return;
  }
  /* "pinwrite" command: read the status of GPIO pin
     Format: pinwrite @03 H
     (03 is the pin number, H is the status to write */
  if(checkCommandType(incomingCommand, "pinwrite")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If the number of parts is wrong */
    if(numberOfParts!=3) {
      /* Complain about the number of parameters */
      Serial.print("The number of parameters is wrong.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* The current byte data */
    uint8_t currentByteData = 0;
    /* Get the second part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* If the second part is not "@AB" (AB is the pin number)
       (Length is not 3, or the first char is not '@', or the remaining two chars are not hex) */
    if((strlen(part)!=3)||(part[0]!='@')||(getByteFromHex(part+1, &currentByteData)!=0)) {
      /* Complain about pin number */
      Serial.print("Invalid pin number.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Save the pin number */
    uint8_t pinNumber = currentByteData;
    /* Get the third part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 2, part, INCOMING_COMMAND_PART_SIZE);
    /* If it's neither H nor L */
    if((strcmp(part, "h")!=0)&&(strcmp(part, "l")!=0)) {
      /* Complain about pin status */
      Serial.print("Invalid pin status.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Whether to write the status of high */
    uint8_t pinWriteHigh = 0;
    /* If it's H */
    if(strcmp(part, "h")==0) {
      pinWriteHigh = 1;
    }
    /* Otherwise */
    else {
      /* It's redundant... don't write high */
      pinWriteHigh = 0;
    }
    /* Set pin mode */
    pinMode(pinNumber, OUTPUT);
    /* Print pin number */
    printByte(pinNumber);
    /* If to set high (it's a verbose way...) */
    if(pinWriteHigh) {
      /* Set high */
      digitalWrite(pinNumber, HIGH);
      /* Print status */
      Serial.write("H");
    }
    /* Otherwise */
    else {
      /* Set low */
      digitalWrite(pinNumber, LOW);
      /* Print status */
      Serial.write("L");
    }
    /* New line */
    Serial.print("\r\n");
    /* Show the "OK" status */
    Serial.print(">Status: OK\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* "pinread" command: read the status of GPIO pin
     Format: pinread @03
     (03 is the pin number) */
  if(checkCommandType(incomingCommand, "pinread")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If the number of parts is wrong */
    if(numberOfParts!=2) {
      /* Complain about the number of parameters */
      Serial.print("The number of parameters is wrong.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* The current byte data */
    uint8_t currentByteData = 0;
    /* Get the second part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* If the second part is not "@AB" (AB is the pin number)
       (Length is not 3, or the first char is not '@', or the remaining two chars are not hex) */
    if((strlen(part)!=3)||(part[0]!='@')||(getByteFromHex(part+1, &currentByteData)!=0)) {
      /* Complain about pin number */
      Serial.print("Invalid pin number.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Save the pin number */
    uint8_t pinNumber = currentByteData;
    /* Print pin number */
    printByte(pinNumber);
    /* Set pin mode */
    pinMode(pinNumber,INPUT);
    /* If high */
    if(digitalRead(pinNumber)==HIGH) {
      /* Print H */
      Serial.print("H");
    }
    /* Otherwise */
    else {
      /* Print L */
      Serial.print("L");
    }
    /* New line */
    Serial.print("\r\n");
    /* Show the "OK" status */
    Serial.print(">Status: OK\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* "spi" command: SPI-bus write and read 
     Format: spi @01M2 11 22 33
     (SS pin 0x01, MSB first, Mode 2) */
  if(checkCommandType(incomingCommand, "spi")) {
    /* Show the "detail" text */
    Serial.print(">Detail: ");
    /* Get the number of parts of command */
    uint16_t numberOfParts = getNumberOfParts(incomingCommand);
    /* If there are too few parameters */
    if(numberOfParts<3) {
      /* Complain about too few parameters */
      Serial.print("Too few parameters.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* One part of the command */
    char part[INCOMING_COMMAND_PART_SIZE] = "";
    /* The current byte data */
    uint8_t currentByteData = 0;
    /* Get the second part (we have already checked the number of parts) */
    getSinglePart(incomingCommand, 1, part, INCOMING_COMMAND_PART_SIZE);    
    /* If the second part is not "@01M2"
       (Length is not 5, or the first char is not '@', or the remaining two chars are not hex) */
    if((strlen(part)!=5)||(part[0]!='@')||(getByteFromHex(part+1, &currentByteData)!=0)) {
      /* Complain about too few parameters */
      Serial.print("The format of SPI settings is wrong, or SS pin number is invalid.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* Save SS pin number */
    uint8_t ssPinNumber = currentByteData;
    /* If the fourth character is neither M nor L */
    if((part[3]!='m')&&(part[3]!='l')) {
      /* Complain about MSB/LSB */
      Serial.print("Invalid MSB/LSB setting.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* MSB first */
    uint8_t msbFirst = 0;
    /* If it's M */
    if(part[3]=='m') {
      /* MSB first */
      msbFirst = 1;
    }
    /* Otherwise */
    else {
      /* LSB first */
      msbFirst = 0;
    }
    /* If the fifth character is not '0'~'3' */
    if((part[4]>'3')||(part[4]<'0')) {
      /* Complain about SPI mode */
      Serial.print("Invalid SPI mode number.\r\n");
      /* Show the "ERR" status */
      Serial.print(">Status: ERR\r\n");
      /* Cancel the remaining process */
      return;
    }
    /* SPI mode */
    uint8_t spiMode = part[4] - '0';
    /* Variable for loop */
    uint16_t i = 0;
    /* Loop through the parts of data to write (we have already checked the number of parts) */
    for(i=2;i<=numberOfParts-1;i++) {
      /* Get the current part */
      getSinglePart(incomingCommand, i, part, INCOMING_COMMAND_PART_SIZE);
      /* If it's not valid hex */
      if(getByteFromHex(part, &currentByteData)!=0) {
        /* Complain about invalid SPI data */
        Serial.print("Invalid SPI data.\r\n");
        /* Show the "ERR" status */
        Serial.print(">Status: ERR\r\n");
        /* Cancel the remaining process */
        return;
      }
    }
    /* Output for SS pin */
    pinMode(ssPinNumber, OUTPUT);
    /* Set high for SS pin */
    digitalWrite(ssPinNumber, HIGH);
    /* Delay a little bit */
    delayMicroseconds(SPI_DELAY_US);
    /* Start SPI (this is a verbose way) 
       If MSB first... */
    if(msbFirst) {
      /* Check SPI mode variable */
      switch(spiMode) {
        /* Mode 0 */
        case 0:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
          /* End this "case" */
          break;
        /* Mode 1 */
        case 1:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
          /* End this "case" */
          break;
        /* Mode 2 */
        case 2:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE2));
          /* End this "case" */
          break;
        /* Mode 3 */
        case 3:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
          /* End this "case" */
          break;
      }
    }
    /* LSB first */
    else {
      /* Check SPI mode variable */
      switch(spiMode) {
        /* Mode 0 */
        case 0:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, LSBFIRST, SPI_MODE0));
          /* End this "case" */
          break;
        /* Mode 1 */
        case 1:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, LSBFIRST, SPI_MODE1));
          /* End this "case" */
          break;
        /* Mode 2 */
        case 2:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, LSBFIRST, SPI_MODE2));
          /* End this "case" */
          break;
        /* Mode 3 */
        case 3:
          /* Start SPI with this setting */
          SPI.beginTransaction(SPISettings(SPI_SPEED, LSBFIRST, SPI_MODE3));
          /* End this "case" */
          break;
      }
    }
    /* Delay a little bit */
    delayMicroseconds(SPI_DELAY_US);
    /* Set SS to low */
    digitalWrite(ssPinNumber, LOW);
    /* Delay a little bit */
    delayMicroseconds(SPI_DELAY_US);
    /* Byte received from SPI */
    uint8_t spiReadData = 0;
    /* Loop through the parts of data to write (we have already checked the number of parts) */
    for(i=2;i<=numberOfParts-1;i++) {
      /* Get the current part */
      getSinglePart(incomingCommand, i, part, INCOMING_COMMAND_PART_SIZE);
      /* Convert it to byte */        
      getByteFromHex(part, &currentByteData);
      /* Write this byte to SPI */
      spiReadData = SPI.transfer(currentByteData);
      /* Print the received byte */
      printByte(spiReadData);
    }
    /* Delay a little bit */
    delayMicroseconds(SPI_DELAY_US);
    /* Set SS to high */
    digitalWrite(ssPinNumber, HIGH);
    /* New line */
    Serial.print("\r\n");
    /* Say OK */
    Serial.print(">Status: OK\r\n");
    /* Cancel the remaining process */
    return;
  }
  /* Wrong command */
  Serial.print(">Detail: Wrong command.\r\n>Status: ERR\r\n");
}

/* Main loop */
void loop() {
  /* If there is received data */
  if (Serial.available() > 0) {
    /* Check if it's out of boundary 
       Max index is "INCOMING_COMMAND_SIZE-1-1" ... Reserve one byte at the end for 0x00*/
    if(incomingCommandIndex>(INCOMING_COMMAND_SIZE-1-1)) {
      /* Send the message indicating wrong length */
      Serial.write("\r\n>Detail: Incoming command is too long.\r\n>Status: ERR\r\n#");
      /* Pause for some time, then discard this incoming command 
         (this has overflow issue if it's running for many days) */
      while(millis()-incomingByteMs<=INCOMING_COMMAND_PAUSE_MS) {
        /* If there is incoming data */
        if(Serial.available() > 0) {
          /* Read the incoming data */
          incomingByte = Serial.read();          
        }
      }
      /* Clear index of incoming command */
      incomingCommandIndex = 0;
      /* Clear the flag of "got first letter"  */
      gotFirstLetter = 0;
      /* Clear the flag of "previous byte is space" */
      uint8_t previousByteIsSpace = 0;
      /* Cancel the remaining process */
      return;
    }
    /* Read the incoming byte */
    incomingByte = Serial.read();
    /* Remember when it's received */
    incomingByteMs = millis();
    /* If it's new-line characters, or tab */
    if((incomingByte=='\r')||(incomingByte=='\n')||(incomingByte=='\t')) {
      /* Treat it as space */
      incomingByte=' ';
    }
    /* If it's space character at the beginning */
    if((!gotFirstLetter)&&(incomingByte==' ')) {
      /* Discard it */
      return;
    }
    /* Got the first letter */
    gotFirstLetter=1;
    /* If the previous byte is space, and the current byte is also space */
    if((previousByteIsSpace)&&(incomingByte==' ')) {
      /* Discard it */
      return;
    }
    /* If the current byte is space */
    if(incomingByte==' ') {
      /* Tell the next round: got space character */
      previousByteIsSpace=1;
    }
    /* Not space character */
    else {
      /* Tell the next round: not space */
      previousByteIsSpace=0;
    }    
    /* Save this incoming byte */
    incomingCommand[incomingCommandIndex] = incomingByte;
    /* Add 0x00 */
    incomingCommand[incomingCommandIndex+1] = 0x00;
    /* Increase the index */
    incomingCommandIndex++;
  }
  /* If the specified time passed without incoming byte,
     (and incoming command is not empty)
     we have a complete command now. 
     (this has overflow issue if it's running for many days) */
  if(((millis()-incomingByteMs)>=INCOMING_COMMAND_MS) 
  &&(incomingCommandIndex>0)) {
    /* Print the incoming command */
    Serial.print(incomingCommand);
    /* Print the new-line characters */
    Serial.print("\r\n");
    /* Execute the incoming command */
    executeIncomingCommand();
    /* Clear index of incoming command */
    incomingCommandIndex = 0;
    /* Clear the flag of "got first letter"  */
    gotFirstLetter = 0;
    /* Clear the flag of "previous byte is space" */
    previousByteIsSpace = 0;
    /* Print "#" (wait for input) */
    Serial.print("#");
  }
}

/* END OF FILE */
