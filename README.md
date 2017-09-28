# Cloud-Edge-Device Architecture based Internet of Things Platform

## Best Course Project - Distributed Operating Systems

## Summary
This project is a prototype of an end-to-end IOT solution to externalize the sensor-based sesvices on Beagle Bone Black platforms running Xinu as an OS. It provides an unique c++ based high-level driver interface to program the sensor devices on a platform. Cloud accesses the services through and onsite edge device which controls a bunch of platforms under its realm. Edge to platform communication is MQTT-SN based and that of Cloud to edge is RESTful.

A c++ based driver interface makes it easy to build services over the sensors providing us with the facilities of resuse, portability and maintence. The idea is unique and novel. A c++ based high-level driver interface gives xinu a powerful and flexible programmable interface instead of its original, rigid and static interface.

## Features
FEATURES:

1. Added GPIO driver to Xinu
2. Added ADC (Analog to Digital Converter) driver to Xinu
3. Introduced a unique C++ highlevel driver. Refer to the architecture diagram
4. Implemented MQTT-SN (over UDP) client in XINU to support communication through pub/sub based model
5. Implemented MQTT-SN broker in edge devices running on linux machines
6. The project demonstrates exposing and accessing the services running on the Beagele Bone Black
   platforms to the cloud. The cloud avails the services through edge devices which controls number of
   platforms in its vicinity. Cloud interacts with the edge devices through a RESTful interface
  
## Team

[Siddharth Gupta](https://siddg.me) - siddg@ufl.edu

[Anurag Bihani](https://anuragbihani.me) - anuragbihani@ufl.edu

[Gyanranjan Hazarika](#) - ghazarika@ufl.edu

[Abhineet Kulkarni](#) - abhineetrkulkarn@ufl.edu

## Copyright

![alt text](https://i.creativecommons.org/l/by-nc-nd/4.0/88x31.png)

This work is licensed under a [Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International License](http://creativecommons.org/licenses/by-nc-nd/4.0/)





