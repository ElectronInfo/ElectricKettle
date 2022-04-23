#include <avr/wdt.h>
#include <avr/eeprom.h>

struct pin {
	volatile uint8_t &ddr;
	volatile uint8_t &port;
	uint8_t bit;
};

const pin LED_R = {DDRB, PORTB, 2};	// Красные светодиоды подсветки
const pin LED_G = {DDRA, PORTA, 7};	// Зеленые светодиоды подсветки
const pin LED_B = {DDRA, PORTA, 6};	// Синие светодиоды подсветки

const pin BUZZER = {DDRA, PORTA, 5};	// Пьезоизлучатель

const pin A_1_2 = {DDRB, PORTB, 0};	// Аноды 1 и 2 светодиодов, ниже также используется (PINB & _BV(0))
const pin A_3_4 = {DDRA, PORTA, 0};	// Аноды 3 и 4 светодиодов, ниже также используется (PINA & _BV(0))
const pin C_1_3 = {DDRA, PORTA, 3};	// Катоды 1 и 3 светодиодов
const pin C_2_4 = {DDRB, PORTB, 1};	// Катоды 2 и 4 светодиодов

const pin RELAY = {DDRA, PORTA, 4};	// Реле

const pin NTC = {DDRA, PORTA, 1}; 	// Аналоговый вход датчика температуры



const int16_t chooseTemp[5] = {4000, 6000, 7000, 8000, 9000};	// Выбор желаемой температуры °C * 100
const uint8_t chooseHV[5] = {86, 140, 172, 187, 227};	// Цвет свечения подсветки при выборе температуры (Hue 0-255)

const uint8_t arrTimeEff[] = {255, 128, 64, 32, 40, 20, 10, 5, 255, 128, 64, 32, 68, 34, 17, 9, 40, 20, 10, 5};	// Массив для счетчика обновления подсветки (скорость эффектов 1-20, меньше - быстрее)





volatile uint32_t timeISR = 0;	// Счетчик времени (подобие millis) ~4 = 1 мс.

uint32_t prevCount = 0;		// Счетчик для проверки кнопок

uint8_t keyPress = 0;		// Нажатие и удержание кнопок
uint8_t numKeyPress = 0;	// Какая кнопка была нажата (сохранение для обработки при отпускании)
uint32_t keyPressCount = 0;	// Счетчик для нажатия/отпускания кнопок

uint8_t kettleMode = 0;		// Режим работы нагревателя(0 - выключен, 1-5 температурные режимы, 99 - неполное кипячение, 100 - кипячение)

int16_t currentTemp;		// Текущая температура (вне диапазона 18-97°C большая погрешность)


// Измерение скорости повышения температуры
enum : uint8_t {
	DIFF_OFF,
	DIFF_START,
	DIFF_CALC,
} diffMode;					// Режим

uint32_t diffCount = 0;		// Счетчик
int16_t diffTemp;			// Предыдущая температура
uint16_t diffCalc;			// Скорость повышения температуры (°C/сек. * 1000)
int16_t diffOffset = -150;	// Слагаемое для температурных режимов


uint32_t boilTimer = 0;		// Таймер выключения для режимов кипячения
uint32_t boilCount = 0;		// Счетчик для таймера выключения

bool chooseTimer = false;	// Включение таймера отсчета после выбора температуры кнопкой
uint8_t chooseMode = 0;		// Выбранная температура (1-5)
uint32_t chooseCount = 0;	// Счетчик выбора температуры

uint32_t enabledCount = 0;	// Время с момента включения нагревателя (для отключения, если долго не выключается сам по температуре)

int16_t lastMaxTemp = 0;	// Последняя максимальная температура
uint32_t lastMaxCount = 0;	// Время последнего обновления температуры (для отключения если долго не изменяется)


uint8_t ledsMode = 0;		// Режим работы подсветки (1-20 - различные режимы, 0 или 21 - цвет в соответствии с текущей температурой)
uint8_t ledsColor = 0;		// Цвет
uint8_t ledsBright = 0;		// Яркость
bool ledsDir = true;		// Изменение яркости (true - увеличивается, false - уменьшается)

uint8_t effectMode = 0; 	// Выбранный режим работы подсветки в режиме просмотра
uint32_t effectCount = 0;	// Счетчик для выключения режима просмотра при длительном бездействии





void pinModePin(pin pin, uint8_t mode)
{
	if(mode == OUTPUT)
	{
		pin.ddr |= _BV(pin.bit);
	}
	else if(mode == INPUT_PULLUP)
	{
		pin.ddr &= ~_BV(pin.bit);
		pin.port |= _BV(pin.bit);
	}
	else	// INPUT
	{
		pin.ddr &= ~_BV(pin.bit);
		pin.port &= ~_BV(pin.bit);	// Внимание! Без этой строки, если пин был настроен, как OUTPUT-HIGH, то при переключении на INPUT будет INPUT_PULLUP, но если не используется, можно сэкономить
	}
}


void digitalWritePin(pin pin, uint8_t val)
{
	if(val)
		pin.port |= _BV(pin.bit);
	else
		pin.port &= ~_BV(pin.bit);
}


void analogWritePin(pin pin, uint8_t val)
{
	/*
	if(val == 0)
	{
		digitalWritePin(pin, LOW);
	}
	else if(val == 255)
	{
		digitalWritePin(pin, HIGH);
	}
	else
	*/
	{
		if(pin.bit == 2)	// PB2
		{
			//TCCR0A &= ~_BV(COM0A0);
			//TCCR0A |= _BV(COM0A1);
			OCR0A = val;
		}
		else if(pin.bit == 7)	// PA7
		{
			//TCCR0A &= ~_BV(COM0B0);
			//TCCR0A |= _BV(COM0B1);
			OCR0B = val;
		}
		else if(pin.bit == 6)	// PA6
		{
			//TCCR1A &= ~_BV(COM1A0);
			//TCCR1A |= _BV(COM1A1);
			OCR1A = val;
		} 
	}
}


ISR(TIM0_OVF_vect)	// Прерывание по переполнению таймера 0
{
	timeISR++;	// Увеличение счетчика

	if(kettleMode)	// Если реле включено, то эмулировать ШИМ на пине для реле (частота ШИМ ~2кГц, при тактовой частоте микроконтроллера 8 МГц)
	{
		RELAY.port ^= _BV(RELAY.bit);	// digitalWrite(RELAY,  !digitalRead(RELAY));
	}
}


uint32_t getCount()	// Счетчик времени (подобие millis) ~4 = 1 мс.
{
	uint32_t val;   
	uint8_t pSREG = SREG;
	cli();
	val = timeISR;
	SREG = pSREG;
	return val;
}


void showRGB(uint8_t r, uint8_t g, uint8_t b)	// Цвет в формате RGB отдельными параметрами
{
	analogWritePin(LED_R, 255-r);
	analogWritePin(LED_G, 255-g);
	analogWritePin(LED_B, 255-b);
}


void offRGB()	// Выключить подсветку
{
	showRGB(0, 0, 0);
}


void showHV(uint8_t hue, uint8_t value = 255)	// Цветовое колесо 0-255 и яркость
{
	uint8_t r, g, b;

	uint8_t i = hue / 43;
	uint8_t f = (hue - (i * 43)) * 6; 

	uint8_t q = ((uint16_t)value * (255 - f)) >> 8;
	uint8_t t = ((uint16_t)value * f) >> 8;

	switch (i)
	{
		case 0:
			r = value;
			g = t;
			b = 0;
			break;
			
		case 1:
			r = q;
			g = value;
			b = 0;
			break;
			
		case 2:
			r = 0;
			g = value;
			b = t;
			break;
			
		case 3:
			r = 0;
			g = q;
			b = value;
			break;
			
		case 4:
			r = t;
			g = 0;
			b = value;
			break;
			
		default:
			r = value;
			g = 0;
			b = q;
	}

	showRGB(r, g, b);
}


uint8_t rand255()	// Случайное число
{
	uint8_t temp = 0;
	for(uint8_t i=0; i<8; i+=2)
	{
		temp |= ((analogRead(NTC.bit)^ledsColor) & 0b11) << i;
	}
	
	return temp;
}


void effectInit()	// Инициализация при включении/смене эффекта
{
	ledsColor = rand255();	// Случайный начальный цвет для эффектов 1-4
	ledsBright = 0;
	//ledsDir = true;
}


void effectOff()	// Выключение эффекта
{
	effectMode = 0;
	offRGB();
}


uint8_t getColorTemp()	// Преобразование температуры 17-95°C в цвет от оранжевого до красного
{
	if(currentTemp < 1829)
		return 7;
	else if(currentTemp > 9500)
		return 0;
	else
		return currentTemp / 31 - 51;
}


void showColorTemp()	// Показать температуру цветом подсветки
{
	showHV(getColorTemp());
}


void showEffectMode(uint8_t mode = ledsMode)	// Эффекты подсветки
{
	static uint32_t ledsCount = 0;
	
	if(mode > 0 && mode < 21)
	{
		if(getCount() - ledsCount >= arrTimeEff[mode-1])
		{
			ledsCount = getCount();
			
			if(mode <= 4)	// Плавная смена цвета
			{
				ledsColor++;
				
				showHV(ledsColor);
			}
			else
			{
				if(ledsBright == 0)
				{
					ledsDir = true;
					
					if(mode <= 12)
						ledsColor += 41 + rand255()%184;	// Генерация случайного цвета
					else if(mode > 16)
						ledsColor = getColorTemp();
				}
				
				if(ledsDir)
				{
					ledsBright++;
					
					if(ledsBright == (mode <= 8 || mode > 16 ? 255 : (mode <= 12 ? 16 : 150)))
					{
						ledsDir = false;
					}
				}
				else
				{
					ledsBright--;
				}

				if(mode <= 8 || mode > 16)	// Вспышки случайного цвета или вспышки цветом соответствующей температуры
					showHV(ledsColor, ledsBright);
				else if(mode <= 12)	// Резкая смена на случайный цвет
					showHV(ledsColor);
				else
					showRGB(ledsBright, ledsBright, ledsBright);	// Вспышки белого цвета
			}
		}
	}
	else
	{
		showColorTemp();
	}
}


void showLedOn(uint8_t maskLeds = chooseMode)	// Включение светодиодов в матрице (1 - первый, 2 - первый и второй, 3 - второй, 4 - третий, 5 - четвертый, иначе никакой)
{
	switch(maskLeds)
	{
		case 1:
		case 2:
		case 3:
			digitalWritePin(A_3_4, LOW); pinModePin(A_3_4, OUTPUT);
			digitalWritePin(A_1_2, HIGH); pinModePin(A_1_2, OUTPUT);
			break;
			
		case 4:
		case 5:
			digitalWritePin(A_1_2, LOW); pinModePin(A_1_2, OUTPUT);
			digitalWritePin(A_3_4, HIGH); pinModePin(A_3_4, OUTPUT);
			break;
	}

	switch(maskLeds)
	{
		case 1:
		case 4:
			pinModePin(C_2_4, INPUT);
			pinModePin(C_1_3, OUTPUT);
			break;
			
		case 2:
			pinModePin(C_1_3, OUTPUT);
			pinModePin(C_2_4, OUTPUT);
			break;
			
		case 3:
		case 5:
			pinModePin(C_1_3, INPUT);
			pinModePin(C_2_4, OUTPUT);
			break;

		default:
			pinModePin(A_1_2, INPUT_PULLUP);
			pinModePin(A_3_4, INPUT_PULLUP);
			pinModePin(C_1_3, INPUT);
			pinModePin(C_2_4, INPUT);
	}
}


void toneBuzzer(uint16_t frequency, uint16_t duration)	// Проиграть звук определенной частоты и длительности
{
	if(frequency)
	{
		uint16_t tDelay = 1000000UL / frequency;	// uint16_t - минимум 16 Гц
		uint16_t durDelay = tDelay / 2 - tDelay/64;
		uint32_t numCycles = 1000UL * duration / tDelay;
		
		for(uint32_t i=0; i<numCycles; i++)
		{
			PORTA &= ~_BV(PA5);
			delayMicroseconds(durDelay);
			PORTA |= _BV(PA5);
			delayMicroseconds(durDelay);
		}
	}
	else
	{
		delay(duration);
	}
}


void kettleOn(uint8_t numMode)	// Включение реле с предварительным отключением всех светодиодов для снижения общей нагрузки на источник питания и инициализация различных параметров
{
	enabledCount = getCount();
	lastMaxCount = getCount();
	lastMaxTemp = 0;
	diffMode = DIFF_OFF;
	diffOffset = -150;
	diffTemp = currentTemp;

	offRGB();
	showLedOn(0);
	digitalWritePin(RELAY, HIGH);
	delayMicroseconds(65535);
	kettleMode = numMode;
	effectInit();
}


void kettleOff()	// Выключение реле и сброс различных параметров
{
	kettleMode = 0;
	boilTimer = 0;
	digitalWritePin(RELAY, LOW);
	offRGB();
	chooseTimer = false;
	chooseMode = 0;
	//showLedOn();
}

void delay250()
{
	delay(250);
}

void delay500()
{
	delay250();
	delay250();
}

void delay1000()
{
	delay250();
	delay250();
	delay250();
	delay250();
}


void playCompleteAndDelay()	// Мелодия выключения чайника
{
	toneBuzzer(2048, 80);
	delay1000();
}


int16_t getTemp()	// Текущая температура °C * 100
{
	static int16_t arr[3];
	static uint8_t n = 0;
	
	arr[n] = analogRead(NTC.bit);
	
	n++;
	n %= 3;
	
	int16_t analog = (arr[0] < arr[1] ? (arr[1] < arr[2] ? arr[1] : (arr[2] < arr[0] ? arr[0] : arr[2])) : (arr[0] < arr[2] ? arr[0] : (arr[2] < arr[1] ? arr[1] : arr[2])));

	return (analog > 558) ? (18291 - analog * 26) : (13820 - analog * 18);
}


void alarmBeepRed()	// Вспышка красного цвета и звуковой сигнал
{
	showHV(0);
	toneBuzzer(2048, 500);
	offRGB();
	delay500();
}



void setup()
{  
	wdt_enable(WDTO_8S);	// Включение сторожевого таймера (WatchDog), 8 секунд
	
	digitalWritePin(RELAY, LOW);
	pinModePin(RELAY, OUTPUT);

	digitalWritePin(BUZZER, HIGH);
	pinModePin(BUZZER, OUTPUT);

	pinModePin(NTC, INPUT);
	
	offRGB();
	
	// Настройка двух таймеров, изменение делителей и режима работы ШИМ, чтобы сделать одинаковые частоты на 3 ШИМ выходах для RGB, хотя это и необязательно
	//TCNT0 = 0;
	//TCNT1 = 0;

	TCCR1A = _BV(WGM10) | _BV(COM1A1);
	TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM01) | _BV(WGM00);
	
	TCCR0B = _BV(CS01);
	TCCR1B = _BV(WGM12) | _BV(CS11);
	
	TIMSK0 = _BV(TOIE0);	// Включение прерывания по переполнению таймера 0
	
	
	/*
	pinModePin(A_1_2, INPUT_PULLUP);
	pinModePin(A_3_4, INPUT_PULLUP);
	pinModePin(C_1_3, INPUT);
	pinModePin(C_2_4, INPUT);
	*/
	showLedOn(0);
	
	toneBuzzer(1024, 50);

	getTemp();
	getTemp();
	
	ledsMode = eeprom_read_byte((uint8_t*)1);	// Считывание режима работы подсветки из ячейки 1 EEPROM
	
	currentTemp = getTemp();
	uint8_t colorFlash = getColorTemp();	// Вспышка цветом в соответствии с температурой
	//uint8_t colorFlash = rand255();		// Вспышка случайным цветом при постановке чайника на подставку
	
	//digitalWritePin(LED_R, HIGH);
	//digitalWritePin(LED_G, HIGH);
	//digitalWritePin(LED_B, HIGH);

	pinModePin(LED_R, OUTPUT);
	pinModePin(LED_G, OUTPUT);
	pinModePin(LED_B, OUTPUT);
	
	uint8_t i = 1;
	bool dir = true;
	while(i != 0)
	{
		if(i == 255)
			dir = false;
		
		if(dir)
		{
			i++;
		}
		else
		{
			i--;
		}
		
		showHV(colorFlash, i);
		delayMicroseconds(5000-(int16_t)i*15);	// Более плавная вспышка (при низкой яркости - медленнее, при высокой - быстрее, так как при высокой яркости изменение почти незаметно)
		//delay(2);
		
		if(!(PINA & _BV(0)) || !(PINB & _BV(0)))	// Чтобы не ждать окончания вспышки, можно включать чайник кнопками
			break;
	}
	
	offRGB();
}



void loop()
{
	if(currentTemp >= 10300)  // Если температура выше указанной, то моргать подсветкой красного цвета и издавать прерывистый сигнал
	{
		kettleOff();

		alarmBeepRed();
		
		return;
	}
	else if(currentTemp < 715)  // Обрыв датчика температуры (715(676) примерно -5°C), 221(695) когда один резистор 47,5кОм без NTC; моргать подсветкой красного цвета
	{
		kettleOff();

		showHV(0);
		delay500();
		offRGB();
		delay500();
		
		return;
	}
	else if(kettleMode)	// Нагреватель включен
	{
		showEffectMode();
		
		if(currentTemp > lastMaxTemp)	// Обновление максимальной температуры
		{
			lastMaxCount = getCount();
			lastMaxTemp = currentTemp;
		}
		
		if(getCount() - lastMaxCount >= 262144)	// Температура не изменяется больше ~65 сек., выключить чайник и воспроизвести сигнал 2 раза
		{
			kettleOff();

			alarmBeepRed();
			alarmBeepRed();
		}
		else if(getCount() - enabledCount >= 2097152)  // Включен более ~8.7 мин., выключить чайник и воспроизвести сигнал 3 раза
		{
			kettleOff();

			alarmBeepRed();
			alarmBeepRed();
			alarmBeepRed();
		}
		else if(currentTemp > 9500)  // Если температура больше указанной (9500 ~ 95.8), начать отсчет таймера
		{
			if(boilTimer == 0)
			{
				boilCount = getCount();
				
				uint32_t diffTime = 65000;	// 27860-91655
				
				if(diffMode == DIFF_CALC)
				{
					if(diffCalc > 3000)
					{
						diffCalc = 3000;
					}
					else if(diffCalc < 272)
					{
						diffCalc = 272;
					}
					
					diffTime = 19082305UL/diffCalc + 21500;
				}
				
				boilTimer = (kettleMode == 99) ? (diffTime / 4) : diffTime;
			}
		}
		else if(kettleMode <= 5)  // Если включен режим подогрева до нужной температуры
		{
			if(currentTemp > chooseTemp[kettleMode-1]+diffOffset)
			{
				kettleOff();
				playCompleteAndDelay();
			}
			else if(diffMode == DIFF_CALC)
			{
				if(diffCalc >= 1300)
				{
					diffOffset = -1180;
				}
				else if(diffCalc > 720)
				{
					diffOffset = 120 - (int16_t)diffCalc;
				}
				else if(diffCalc > 200)
				{
					diffOffset = (int16_t)(253796UL/diffCalc) - 933;
				}
				else
				{
					diffOffset = 350;
				}
			}
		}

		if(diffMode == DIFF_OFF)
		{
			if(currentTemp > diffTemp+100)
			{
				diffCount = getCount();
				diffMode = DIFF_START;
				diffTemp = currentTemp;
			}
		}
		else
		{
			uint32_t diffTime = getCount() - diffCount;
			
			if(diffMode == DIFF_START)
			{
				if(diffTime >= 4096)	// ~4000 - 1 сек.
				{
					diffMode = DIFF_CALC;
				}
			}
			
			if(diffMode == DIFF_CALC)
			{
				if(currentTemp > diffTemp)
				{
					uint32_t speedCalc = (currentTemp - diffTemp)*40000UL / diffTime;
					
					if(diffTime < 40000)	// Если с момента начала повышения температуры прошло менее 10 сек.
					{
						speedCalc = speedCalc * 25536 / (65536 - diffTime);	// Делить полученное значение на 2.5 и плавно до 1 (без изменений)
					}
					
					diffCalc = speedCalc;	// 1000 - 1.00 градус в секунду
				}
				else
				{
					diffCalc = 0;
				}
			}
		}
		

		if(boilTimer != 0)	// Если начался отсчет времени таймера выключения
		{
			if(getCount() - boilCount >= boilTimer)
			{
				kettleOff();
				playCompleteAndDelay();
			}
		}
	}
	else if(effectMode)	// Если включен режим просмотра эффекта
	{
		if(getCount() - effectCount < 4500000)	// ~20 минут
		{
			showEffectMode(effectMode);
		}
		else
		{
			effectOff();
		}
	}


	if(getCount() - prevCount >= 20)	// Обновление температуры и обработка нажатий кнопок
	{
		prevCount = getCount();
		
		wdt_reset();  // Сброс сторожевого таймера
		
		showLedOn(0);
		
		currentTemp += (getTemp() - currentTemp)/32;
		//asm("nop");	// Небольшая задержка между изменением состояния выводов и считыванием
		
		uint8_t nPress = 0;

		if(!(PINA & _BV(0)))
			nPress = 1;
		else if(!(PINB & _BV(0)))
			nPress = 2;

		showLedOn();
		
		if(nPress)
		{
			if(getCount()-keyPressCount >= (keyPress ? (keyPress > 1 ? 12000 : 3000) : 200))	// Длительное удержание ~3 сек., 0,75 сек. и антидребезг 50 мс.
			{
				keyPressCount = getCount();
				numKeyPress = nPress;

				toneBuzzer(512, 50);
				
				switch(nPress)
				{
					case 1:	// Первая кнопка
						if(!effectMode)	// Режим просмотра эффектов выключен
						{
							if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
							{
								showHV(keyPress == 0 ? 0 : 235);
							}
							else if(kettleMode >= 99 && keyPress == 1)
							{
								kettleMode = kettleMode == 99 ? 100 : 99;
								
								showHV(kettleMode == 100 ? 0 : 235);
								delay1000();
							}
						}
						break;
						
					case 2:	// Вторая кнопка
						if(!effectMode)	// Режим просмотра эффектов выключен
						{
							if(keyPress == 1)	// Недолгое удержание
							{
								if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
								{
									showColorTemp();	// Показать температуру цветом подсветки

									int16_t temp = currentTemp + 50;	// и количеством вспышек светодиодов, 1 вспышка - 1 градус
									for(int8_t i=4; i>=0; i--)
									{
										if(temp >= chooseTemp[i])
										{
											for(int8_t j=0, jj=(temp-chooseTemp[i])/100+1; j<jj; j++)
											{
												showLedOn(i+1);
												
												if(j == 0)
													delay1000();
												else
													delay250();
												
												showLedOn(0);
												delay250();
												
												wdt_reset();
											}
											break;
										}
									}
									/*
									int16_t temp = currentTemp + 250;	// и количеством вспышек светодиодов, 1 вспышка - 5 градусов
									for(int8_t i=4; i>=0; i--)
									{
										if(temp >= chooseTemp[i])
										{
											for(int8_t j=0, jj=(temp-chooseTemp[i])/500+1; j<jj; j++)
											{
												showLedOn(i+1);
												
												if(j == 0)
													delay1000();
												else
													delay500();
												
												showLedOn(0);
												delay500();
											}
											break;
										}
									}
									*/
									delay1000();
									
									offRGB();
								}
								else if(chooseMode)	// Режим подогрева
								{
									eeprom_update_byte((uint8_t*)0, chooseMode);	// Сохранить выбранную температуру в ячейку 0 EEPROM
									showLedOn(0);	// Погасить светодиоды на время для индикации сохранения
									delay250();
								}
							}
							else if(keyPress == 2)	// Удержание более 3 сек.
							{
								if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
								{
									effectMode = 1;	// Включить режим просмотра эффектов
									effectCount = getCount();
									effectInit();
								}
							}
						}
						else	// Режим просмотра эффектов
						{
							if(keyPress == 1)	// Недолгое удержание
							{
								eeprom_update_byte((uint8_t*)1, effectMode);	// Сохранить выбранный эффект в ячейку 1 EEPROM
								ledsMode = effectMode;
								offRGB();	// Погасить подсветку на время для индикации сохранения
								delay250();
							}
						}
						break;          
				}

				if(keyPress < 2)
					keyPress++;
			}
		}
		else if(keyPress)	// Кнопка отпущена
		{
			if(getCount()-keyPressCount >= 200)	// Антидребезг 50 мс
			{
				if(numKeyPress == 1)	// Была нажата кнопка 1
				{
					if(!effectMode)	// Режим просмотра эффектов выключен
					{
						if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
						{
							delay1000();
							
							kettleOn(keyPress < 2 ? 100 : 99);	// Запустить кипячение/неполное кипячение
							
							if(currentTemp >= 9750)  // Температура >= 99°C(9750), начать отсчет 8 секунд
							{
								//if(boilTimer == 0)
								{
									boilCount = getCount();
									boilTimer = 32000;
								}
							}
						}
						else if(keyPress < 2)
						{
							kettleOff();	// Выключить чайник
						}
					}
					else	// Режим просмотра эффектов
					{
						effectOff();	// Выйти из режима просмотра эффектов
					}
				}
				else if(numKeyPress == 2)	// Была нажата кнопка 2
				{
					if(keyPress < 2)	// Кнопка не удерживалась (менее 1 сек.)
					{
						if(!effectMode)	// Режим просмотра эффектов выключен
						{
							//if(kettleMode == 0)
							{
								if(chooseMode == 0)	// Если это первое нажатие, то загрузить выбранную температуру из EEPROM
								{
									chooseMode = eeprom_read_byte((uint8_t*)0);
									if(chooseMode < 1 || chooseMode > 5)
									{
										chooseMode = 1;
									}
								}
								else	// Иначе изменять по кругу от 1 до 5
								{
									chooseMode = chooseMode%5+1;
								}
								
								showLedOn();
								showHV(chooseHV[chooseMode-1]);
								chooseCount = getCount();
								chooseTimer = true;
							}
						}
						else	// Режим просмотра эффектов
						{
							effectMode = effectMode%21+1;	// Смена эффектов по кругу от 1 до 21
							effectCount = getCount();
							effectInit();
						}
					}
				}
				
				keyPressCount = getCount();
				keyPress = 0;
			}
		}
	}

	if(chooseTimer)	// Если включен таймер выбора температуры
	{
		if(getCount()-chooseCount >= 12000) // более 3 сек.
		{
			chooseTimer = false;	// Таймер отключить
			
			if(chooseMode && currentTemp < chooseTemp[chooseMode-1]-350)	// Если температура меньше выбранной
			{
				kettleOn(chooseMode);	// Включить нагрев
			}
			else
			{
				kettleOff();	// Выключить нагрев, если был включен
			}
		}
	}
}
