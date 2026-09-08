// ============================================================
// 2018 HONDA FAN 125 TACHOMETER
// Arduino Nano -> 2023 Titan Blackout Dashboard
//
// D2 = motorcycle pulse input
// D9 = signal output to the dashboard
//
// Each table entry represents a 250 RPM range.
// The frequency values are TEMPORARY and must be
// replaced with the values determined through testing.
// ============================================================


// ============================================================
// CONFIGURATION
// ============================================================

const byte INPUT_PIN = 2;
const byte OUTPUT_PIN   = 9;

// Number of events detected by the sensor per revolution.
// Value already calibrated during previous tests.
const float PULSES_PER_REVOLUTION = 9.2;

// Ignores pulses that occur less than 500 microseconds apart.
const unsigned long FILTER_US = 500;


// ============================================================
// FREQUENCY TABLE
// ============================================================

const float frequencies[44] = {

  8.800,   // [00] 250 - 499
  8.828,   // [01] 500 - 749
  8.856,   // [02] 750 - 999
  8.884,   // [03] 1000 - 1249
  8.912,   // [04] 1250 - 1499
  8.940,   // [05] 1500 - 1749
  8.967,   // [06] 1750 - 1999
  8.995,   // [07] 2000 - 2249
  9.023,   // [08] 2250 - 2499
  9.051,   // [09] 2500 - 2749
  9.079,   // [10] 2750 - 2999
  9.107,   // [11] 3000 - 3249
  9.135,   // [12] 3250 - 3499
  9.163,   // [13] 3500 - 3749
  9.191,   // [14] 3750 - 3999
  9.219,   // [15] 4000 - 4249
  9.247,   // [16] 4250 - 4499
  9.274,   // [17] 4500 - 4749
  9.302,   // [18] 4750 - 4999
  9.330,   // [19] 5000 - 5249
  9.358,   // [20] 5250 - 5499
  9.386,   // [21] 5500 - 5749
  9.414,   // [22] 5750 - 5999
  9.442,   // [23] 6000 - 6249
  9.470,   // [24] 6250 - 6499
  9.498,   // [25] 6500 - 6749
  9.526,   // [26] 6750 - 6999
  9.553,   // [27] 7000 - 7249
  9.581,   // [28] 7250 - 7499
  9.609,   // [29] 7500 - 7749
  9.637,   // [30] 7750 - 7999
  9.665,   // [31] 8000 - 8249
  9.693,   // [32] 8250 - 8499
  9.721,   // [33] 8500 - 8749
  9.749,   // [34] 8750 - 8999
  9.777,   // [35] 9000 - 9249
  9.805,   // [36] 9250 - 9499
  9.832,   // [37] 9500 - 9749
  9.860,   // [38] 9750 - 9999
  9.888,   // [39] 10000 - 10249
  9.916,   // [40] 10250 - 10499
  9.944,   // [41] 10500 - 10749
  9.972,   // [42] 10750 - 10999
  10.000   // [43] 11000+
};


// ============================================================
// VARIABLES
// ============================================================

volatile unsigned long pulseCount = 0;
volatile unsigned long lastPulse = 0;

float filteredRpm = 0.0;
float currentFrequency = 0.0;


// ============================================================
// SENSOR INTERRUPT
// ============================================================

void countPulse() {

  unsigned long now = micros();

  if (now - lastPulse >= FILTER_US) {

    pulseCount++;

    lastPulse = now;
  }
}


// ============================================================
// SELECTS THE DASHBOARD RANGE
// ============================================================
//
// 250-499   -> position 0
// 500-749   -> position 1
// 750-999   -> position 2
// etc.
//
// ============================================================

float convertRpm(float rpm) {

  // Below 250 RPM = dashboard stopped
  if (rpm < 250.0) {
    return 0.0;
  }

  int index = (int)((rpm - 250.0) / 250.0);

  // Protection against exceeding the table bounds
  if (index < 0) {
    index = 0;
  }

  if (index >= 44) {
    index = 43;
  }

  return frequencies[index];
}


// ============================================================
// CONFIGURE TIMER1
// ============================================================
//
// D9 = OC1A
//
// Timer1 directly generates a square wave in hardware.
// This allows decimal frequencies without delayMicroseconds()
// and without blocking sensor readings.
//
// Frequency:
//
// F_CPU / (2 * prescaler * (OCR1A + 1))
//
// Prescaler = 64
// ============================================================

void setFrequency(float frequency) {

  if (frequency <= 0.0) {

    // Turn off Timer1 output
    TCCR1A &= ~(1 << COM1A0);

    digitalWrite(OUTPUT_PIN, LOW);

    currentFrequency = 0.0;

    return;
  }


  // Calculate OCR1A for the desired frequency
  unsigned long value =
    (unsigned long)((16000000.0 /
    (2.0 * 64.0 * frequency)) - 1.0);


  // Safety limits
  if (value > 65535) {
    value = 65535;
  }

  if (value < 1) {
    value = 1;
  }


  // Timer1 in CTC mode
  TCCR1A = 0;
  TCCR1B = 0;

  // CTC
  TCCR1B |= (1 << WGM12);

  // Toggle OC1A = D9
  TCCR1A |= (1 << COM1A0);

  // Compare value
  OCR1A = (uint16_t)value;

  // Prescaler 64
  TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << CS10);

  currentFrequency = frequency;
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(INPUT_PIN, INPUT);

  pinMode(OUTPUT_PIN, OUTPUT);

  digitalWrite(OUTPUT_PIN, LOW);


  // Sensor interrupt
  attachInterrupt(
    digitalPinToInterrupt(INPUT_PIN),
    countPulse,
    RISING
  );


  // Start with the output turned off
  setFrequency(0.0);
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  static unsigned long lastCalculation = 0;


  // Update RPM every 100 ms
  if (millis() - lastCalculation >= 100) {


    // --------------------------------------------------------
    // Copy pulse count
    // --------------------------------------------------------

    noInterrupts();

    unsigned long count = pulseCount;

    pulseCount = 0;

    interrupts();


    // --------------------------------------------------------
    // Calculate pulses per second
    // --------------------------------------------------------

    float pulsesPerSecond =
      count * 10.0;


    // --------------------------------------------------------
    // Calculate RPM
    // --------------------------------------------------------

    float instantRpm =
      (pulsesPerSecond * 60.0)
      / PULSES_PER_REVOLUTION;


    // --------------------------------------------------------
    // Filter
    // --------------------------------------------------------

    filteredRpm =
      (filteredRpm * 0.40) +
      (instantRpm * 0.60);


    // --------------------------------------------------------
    // Find the corresponding range
    // --------------------------------------------------------

    currentFrequency =
      convertRpm(filteredRpm);


    // --------------------------------------------------------
    // Send the corresponding frequency to the dashboard
    // --------------------------------------------------------

    setFrequency(currentFrequency);


    // --------------------------------------------------------
    // Serial Monitor
    // --------------------------------------------------------

    Serial.print("REAL RPM: ");
    Serial.print(filteredRpm, 0);

    Serial.print(" | RANGE: ");

    if (filteredRpm < 250) {

      Serial.print("-");

    } else {

      int index =
        (int)((filteredRpm - 250.0) / 250.0);

      if (index < 0)
        index = 0;

      if (index >= 44)
        index = 43;

      Serial.print(index);
    }

    Serial.print(" | FREQUENCY SENT: ");
    Serial.println(currentFrequency, 3);


    lastCalculation = millis();
  }
}

// by qubitdot
