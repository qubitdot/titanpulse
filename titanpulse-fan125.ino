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

const byte PINO_ENTRADA = 2;
const byte PINO_SAIDA   = 9;

// Number of events detected by the sensor per revolution.
// Value already calibrated during previous tests.
const float PULSOS_POR_VOLTA = 9.2;

// Ignores pulses that occur less than 500 microseconds apart.
const unsigned long FILTRO_US = 500;


// ============================================================
// FREQUENCY TABLE
// ============================================================

const float frequencias[44] = {

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

volatile unsigned long pulsos = 0;
volatile unsigned long ultimoPulso = 0;

float rpmFiltrado = 0.0;
float frequenciaAtual = 0.0;


// ============================================================
// SENSOR INTERRUPT
// ============================================================

void contarPulso() {

  unsigned long agora = micros();

  if (agora - ultimoPulso >= FILTRO_US) {

    pulsos++;

    ultimoPulso = agora;
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

float converterRPM(float rpm) {

  // Below 250 RPM = dashboard stopped
  if (rpm < 250.0) {
    return 0.0;
  }

  int indice = (int)((rpm - 250.0) / 250.0);

  // Protection against exceeding the table bounds
  if (indice < 0) {
    indice = 0;
  }

  if (indice >= 44) {
    indice = 43;
  }

  return frequencias[indice];
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

void configurarFrequencia(float frequencia) {

  if (frequencia <= 0.0) {

    // Turn off Timer1 output
    TCCR1A &= ~(1 << COM1A0);

    digitalWrite(PINO_SAIDA, LOW);

    frequenciaAtual = 0.0;

    return;
  }


  // Calculate OCR1A for the desired frequency
  unsigned long valor =
    (unsigned long)((16000000.0 /
    (2.0 * 64.0 * frequencia)) - 1.0);


  // Safety limits
  if (valor > 65535) {
    valor = 65535;
  }

  if (valor < 1) {
    valor = 1;
  }


  // Timer1 em CTC
  TCCR1A = 0;
  TCCR1B = 0;

  // CTC
  TCCR1B |= (1 << WGM12);

  // Toggle OC1A = D9
  TCCR1A |= (1 << COM1A0);

  // Valor de comparação
  OCR1A = (uint16_t)valor;

  // Prescaler 64
  TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << CS10);

  frequenciaAtual = frequencia;
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(PINO_ENTRADA, INPUT);

  pinMode(PINO_SAIDA, OUTPUT);

  digitalWrite(PINO_SAIDA, LOW);


  // Interrupção do sensor
  attachInterrupt(
    digitalPinToInterrupt(PINO_ENTRADA),
    contarPulso,
    RISING
  );


  // Começa com saída desligada
  configurarFrequencia(0.0);
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  static unsigned long ultimoCalculo = 0;


  // Atualiza RPM a cada 100 ms
  if (millis() - ultimoCalculo >= 100) {


    // --------------------------------------------------------
    // Copia quantidade de pulsos
    // --------------------------------------------------------

    noInterrupts();

    unsigned long quantidade = pulsos;

    pulsos = 0;

    interrupts();


    // --------------------------------------------------------
    // Calcula pulsos por segundo
    // --------------------------------------------------------

    float pulsosPorSegundo =
      quantidade * 10.0;


    // --------------------------------------------------------
    // Calcula RPM
    // --------------------------------------------------------

    float rpmInstantaneo =
      (pulsosPorSegundo * 60.0)
      / PULSOS_POR_VOLTA;


    // --------------------------------------------------------
    // Filtro
    // --------------------------------------------------------

    rpmFiltrado =
      (rpmFiltrado * 0.40) +
      (rpmInstantaneo * 0.60);


    // --------------------------------------------------------
    // Procura o quadradinho correspondente
    // --------------------------------------------------------

    frequenciaAtual =
      converterRPM(rpmFiltrado);


    // --------------------------------------------------------
    // Manda a frequência correspondente para o painel
    // --------------------------------------------------------

    configurarFrequencia(frequenciaAtual);


    // --------------------------------------------------------
    // Monitor Serial
    // --------------------------------------------------------

    Serial.print("RPM REAL: ");
    Serial.print(rpmFiltrado, 0);

    Serial.print(" | QUADRADO: ");

    if (rpmFiltrado < 250) {

      Serial.print("-");

    } else {

      int indice =
        (int)((rpmFiltrado - 250.0) / 250.0);

      if (indice < 0)
        indice = 0;

      if (indice >= 44)
        indice = 43;

      Serial.print(indice);
    }

    Serial.print(" | Hz ENVIADO: ");
    Serial.println(frequenciaAtual, 3);


    ultimoCalculo = millis();
  }
}

// by qubitdot
