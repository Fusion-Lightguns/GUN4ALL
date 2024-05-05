/*!
 * @file simpleDemo.ino
 * @brief DFRobot's Positioning ir camera
 * @n [Get the module here](http://www.dfrobot.com/index.php?route=product/product&product_id=1088)
 * @n This example get the four lightest positions of the IR sources.
 * @n [Connection and Diagram](http://www.dfrobot.com/wiki/index.php/Positioning_ir_camera#Connection_Diagram)
 *
 * @copyright	[DFRobot](http://www.dfrobot.com), 2016
 * @copyright	GNU Lesser General Public License
 *
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 * @version  V1.0
 * @date  2016-02-17
 */

#include "Arduino.h"
#include "Wire.h"

/*!
 * @brief Library for DFRobot's IR Position Camera
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 */
#include "DFRobotIRPosition.h"

DFRobotIRPosition myDFRobotIRPosition;

int positionX[4];     ///< Store the X position
int positionY[4];     ///< Store the Y position

/*!
 *  @brief Print the position result.
 */
void printResult();

void setup()
{
  Serial.begin(19200);
  
  /*!
   *  @brief initailize the module.
   */
  myDFRobotIRPosition.begin();
}

void loop()
{
  /*!
   *  @brief request the position
   */
  myDFRobotIRPosition.requestPosition();
  
  /*!
   *  @brief If there is data available, print it. Otherwise show the error message.
   */
  if (myDFRobotIRPosition.available()) {
    for (int i=0; i<4; i++) {
      positionX[i]=myDFRobotIRPosition.readX(i);
      positionY[i]=myDFRobotIRPosition.readY(i);
    }

    printResult();
  }
  else{
    Serial.println("Device not available!");
  }
  
  delay(50);
}

/*!
 *  @brief Print the position result.
 */
void printResult()
{
  for (int i=0; i<4; i++) {
    Serial.print(positionX[i]);
    Serial.print(",");
    
    Serial.print(positionY[i]);
    Serial.print(";");
  }
  Serial.println();
}







