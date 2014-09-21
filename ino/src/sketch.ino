// pin numbers
const int pin_lcd_backlight[] = {10, 9, 8, 7, 6, 5};
const int pin_vend[] = {40, 41, 42, 43, 47, 48};
const int pin_endstop[] = {28, 29, 30, 31, 32, 33};
const int pin_empty[] = {34, 35, 36, 37, 38, 39};
const int pin_motor[] = {22, 23, 24, 25, 26, 27};
const int pin_mouth_red = 4;
const int pin_mouth_green = 3;
const int pin_mouth_blue = 2;

// global vars
int status_code = 0;
int status_number = 0;
int status_endstop = HIGH;
int error_code = 0;
long error_time = 0;
long endstop_debounce = 0;

long motor_burn_out_safety_time = 0;
long debug_last_millis = 0;

int mouth_fader = 0;
int mouth_fader_direction = 1;
long mouth_fader_last_millis = 0;
int mouth_activity_red = 0;

void setup() {
	// RGB pin setup
	pinMode(pin_mouth_red, OUTPUT);
	pinMode(pin_mouth_green, OUTPUT);
	pinMode(pin_mouth_blue, OUTPUT);

	// 1..6 pin setup
	for(int i = 0; i < 6; i++) {
		pinMode(pin_vend[i], INPUT_PULLUP);
		pinMode(pin_endstop[i], INPUT_PULLUP);
		pinMode(pin_empty[i], INPUT_PULLUP);
		pinMode(pin_motor[i], OUTPUT);
		pinMode(pin_lcd_backlight[i], OUTPUT);
		digitalWrite(pin_motor[i], HIGH);
		digitalWrite(pin_lcd_backlight[i], LOW);
	}

	// LED self-test
	// basically a 'bootup animation'
	for(int i = 0; i <= 6; i++) {
		for(int v = 0; v < 256; v++) {
			if(i < 6) {
				analogWrite(pin_lcd_backlight[i], v);
			}
			if(i > 0) {
				analogWrite(pin_lcd_backlight[i-1], 255-v);
			}
			switch(i) {
				case 0: analogWrite(pin_mouth_red, v); break;
				case 1: analogWrite(pin_mouth_red, 255-v); break;

				case 3: analogWrite(pin_mouth_green, v); break;
				case 4: analogWrite(pin_mouth_green, 255-v); break;

				case 6: analogWrite(pin_mouth_blue, v); break;
			}
			delay(1);
		}
	}

	// serial port initialisation
	Serial.begin(115200);
}

// see http://www.instructables.com/id/two-ways-to-reset-arduino-in-software/step2/using-just-software/
void(* resetFunc) (void) = 0; //declare reset function @ address 0

void loop() {
	// safety check, prevent motors from burning out (does actually happen!)
	if(motor_burn_out_safety_time + 5000 < millis() && status_code != 0) {
		for(int i = 0; i < 6; i++) {
			digitalWrite(pin_motor[i], HIGH);
		}
		status_code = 0;
		digitalWrite(pin_mouth_red, HIGH);
		digitalWrite(pin_mouth_green, LOW);
		digitalWrite(pin_mouth_blue, LOW);
		for(int i = 0; i<10; i++) {
			for(int j = 0; j < 6; j++) {
				digitalWrite(pin_lcd_backlight[j], LOW);
			}
			delay(500);
			for(int j = 0; j < 6; j++) {
				digitalWrite(pin_lcd_backlight[j], HIGH);
			}
			delay(500);
		}
		setup();
	}


	// fade the mouth between blue and green
	if(mouth_fader_last_millis + 10 < millis()) {
		mouth_fader += mouth_fader_direction;
		if(mouth_fader >= 255) {
			mouth_fader_direction = -1;
		} else if(mouth_fader <= 0) {
			mouth_fader_direction = 1;
		}

		int red = 0;
		int green = mouth_fader;
		int blue = 255-mouth_fader;

		mouth_fader_last_millis = millis();

		// but if the status is nonzero, add red flashes
		if(status_code > 0) {
			mouth_activity_red++;
			if(mouth_activity_red < 40) {
				red = 255;
				green = 0;
				blue = 0;
			}
			if(mouth_activity_red > 80) {
				mouth_activity_red = 0;
			}
		}
		// also when there's any error
		else if(error_code > 0) {
			mouth_activity_red++;
			if(mouth_activity_red < 10) {
				red = 255;
				green = 0;
				blue = 0;
			}
			if(mouth_activity_red > 20) {
				mouth_activity_red = 0;
			}
			if(error_time + 2000 < millis()) {
				error_code = 0;
			}
		}

		analogWrite(pin_mouth_red, red);
		analogWrite(pin_mouth_green, green);
		analogWrite(pin_mouth_blue, blue);
	}

	// if the status is zero, listen for button presses
	if(status_code == 0) {
		for(int i = 0; i < 6; i++) {
			if(digitalRead(pin_vend[i]) == LOW) {
				if(digitalRead(pin_empty[i]) == HIGH) {
					error_code = 1;
					error_time = millis();
				} else {
					status_code = 1;
					status_number = i;
					motor_burn_out_safety_time = millis();
					digitalWrite(pin_motor[i], LOW);
				}
			}
		}
	}
	// if the status is 3, assume we are finished
	else if(status_code == 3) {
		status_code = 0;
		digitalWrite(pin_motor[status_number], HIGH);
	}
	// increment status when we've hit the endstop enough times
	// use debouncing for accurate counting
	else {
		int endstop = digitalRead(pin_endstop[status_number]);
		if(endstop_debounce + 50 < millis() && endstop != status_endstop) {
			status_endstop = endstop;
			if(endstop == LOW) {
				status_code++;
				Serial.println("Ploink! "+String(status_code));
			}
			endstop_debounce = millis();
		}
	}

	// set the lcd brightness depending on status, button pressed and empty
	for(int i=0; i<6; i++) {
		if(status_code > 0 && status_number == i) {
			analogWrite(pin_lcd_backlight[i], 255);
		} else if(digitalRead(pin_vend[i]) == LOW) {
			analogWrite(pin_lcd_backlight[i], 128);
		} else if(digitalRead(pin_empty[i]) == LOW) {
			analogWrite(pin_lcd_backlight[i], 128);
		} else {
			analogWrite(pin_lcd_backlight[i], 64);
		}
	}

	// print debug info each 100ms
	if(debug_last_millis + 100 < millis()) {
		String out = "\r\n\r\nEndstop ";
		for(int i = 0; i < 6; i++) {
			if(digitalRead(pin_endstop[i]) == LOW) {
				out += "#";
			} else {
				out += "_";
			}
			out += " ";
		}

		out += "\r\nEmpty   ";
		for(int i = 0; i < 6; i++) {
			if(digitalRead(pin_empty[i]) == LOW) {
				out += "#";
			} else {
				out += "_";
			}
			out += " ";
		}

		out += "\r\nVending ";
		for(int i = 0; i < 6; i++) {
			if(digitalRead(pin_vend[i]) == LOW) {
				out += "#";
			} else {
				out += "_";
			}
			out += " ";
		}

		out += "\r\nStatus  " + String(status_code);

		Serial.println(out);
		debug_last_millis = millis();
	}
}
