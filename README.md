# xGrow - All-In-One Plant Monitoring and Care System

## INTRODUCTION
Maintaining optimal plant health can be a challenge for individuals due to busy schedules and limited
understanding of plant care requirements. 

To address this issue, our project, xGrow, presents an all-in-one IoT -enabled plant monitoring and care system designed for indoor gardening. The system
integrates real-time environmental monitoring and automated control features, reducing user effort
while ensuring optimal conditions for plant growth. 

xGrow utilizes sensors to measure soil moisture, temperature, humidity, and light levels. 
The data collected is processed by an ESP32 microcontroller and displayed via an AWS-hosted website. The system features automatic watering and lighting
controls, along with alerts for water shortages via an LED and buzzer . Additionally, Bluetooth
connectivity allows users to manually control the watering and lighting functions through a mobile
device. 

Our project emphasizes affordability, functionality, and ease of use, offering a practical solution
for plant care. xGrow demonstrates the potential for smart systems to simplify plant care and improve
outcomes for hobbyists and enthusiasts alike.

## SYSTEM ARCHITECTURE

<img width="599" alt="image" src="https://github.com/user-attachments/assets/aa312261-d940-4be9-af02-803ec4869ed4" />

Sensor and Data Collection: Our system includes a Photoresistor , Soil Moisture Sensor , and the DHT 20.
These components collect data about the plant's environment and send these information to the LIL YGO
microcontroller .

Alert System: The red LED and Buzzer serves as an alert, they will alert to notify the user when the watering
system needs more water with visual and audible ques.
Watering System: The servo allows the watering system to provide water to the plant.
Lighting System: The lighting system will provide the plant with light if needed based on the data collected
on the environment.

Cloud Integration: Temperature and humidity data are sent to the EC2 Server via WiFi, where users can see
the last 20 seconds of data updated in real time.
Bluetooth Control: Users can also use their mobile device to control the watering system and lighting
through bluetooth.

## DEMO VIDEO
https://drive.google.com/file/d/150TBBn7Wp8FPF3ztS__qUbtNUBNf1Gp2/view?usp=sharing
