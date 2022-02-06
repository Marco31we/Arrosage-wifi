// Variadic macros used to print information in de-bugging mode
// from LarryD, Arduino forum

#pragma once

#define DBG 2 // 0: no trace, 1=information, 2=Debug, 3=Simulation 
// un-comment this line to print the debugging statements HTTPS
#define DEBUG


#ifdef DEBUG
  #define DPRINT(...)    Serial.print(__VA_ARGS__)
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  // define blank line
  #define DPRINT(...)
  #define DPRINTLN(...)
#endif


#if DBG > 0 // 1=information, 2=Debug, 3=Simulation 
  #define D1PRINT(...)    Serial.print(__VA_ARGS__)
  #define D1PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define D1WRITE(...)    Serial.write(__VA_ARGS__)
#else
  // define blank line
  #define D1PRINT(...)
  #define D1PRINTLN(...)
  #define D1WRITE(...)
#endif

#if DBG > 1 // 2=Debug, 3=Simulation
  #define D2PRINT(...)    Serial.print(__VA_ARGS__)
  #define D2PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  // define blank line
  #define D2PRINT(...)
  #define D2PRINTLN(...)
#endif

#if DBG > 2 // Simulation
  #define D3PRINT(...)    Serial.print(__VA_ARGS__)
  #define D3PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  // define blank line
  #define D3PRINT(...)
  #define D3PRINTLN(...)
#endif
