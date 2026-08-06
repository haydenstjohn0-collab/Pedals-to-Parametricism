The following is the logics and code used in the physical computing project, pedals to parametricism. The following Github entry provides that code as well as basic fundamentals for the hardware. 

Concept: A physical computing module that serves as a prototype for larger dynamic installations. Intended initially to be the size to fit on a desktop, it is imagined that the same logics and even code base could be scaled up with similar hardware to serve as the spatial configurations for live music events. In essence, the project takes a live audio track and translates it into physical motion (stepper motors connected to line connected to a tensile fabric). These same concepts can be used down the line in other physical computing projects in other configurations or physical transformations as is desired or called for by the type of music or spatial configurations desired. 

Physical Components:
  aluminum frame
  stepper motors
  arduino
  breadboards or wiring harness
  power supply (enough to power the desired amount of stepper motors)
  tensile fabric
  rigid line

Basic instructions: Construct a frame using aluminum profile members. On this frame, attach 16 stepper motors, 4 in a line on each edge of the rectangle at even lengths along the aluminum frame. Attached to each of these stepper motors, attach the rigid line. Connect the rigid line to the tensile fabric, stitched in a cylinder, at 4 points opposed to each other to hold the fabric in suspension. This should be done across all 16 motors and all line should be tight. The tensile fabric should be suspended under a little bit of tension across the entire length of the center of the frame. Wire all 16 stepper motors to the Arduino. Wire so that the 4 stepper motors in line with each other connected at the same length to the fabric all receive the same input from the Arduino (one output is split to the 4 stepper motors, this can be done with a bus or simply on a breadboard). Repeat this for the other 3 modules. There should only be 4 outputs. Each output has 4 associated stepper motors. Wire each stepper motor to a sufficient power source and connect Arduino to a computer that has access to TouchDesigner.

Software: For the Arduino code reference file titled: Arduino code provided. This will be uploaded onto the Arduino then TouchDesigner will send the actual data to the machine. Once Arduino code is running, open TouchDesigner and load the file titled: TouchDesigner script provided. Insert an MP3 file to the audio in input and play. The machine should do the rest. 
