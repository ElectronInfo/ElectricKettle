#include <avr/wdt.h>
#include <avr/eeprom.h>

const uint8_t LED_R = PIN_PB2;	// Красные светодиоды подсветки
const uint8_t LED_G = PIN_PA7;	// Зеленые светодиоды подсветки
const uint8_t LED_B = PIN_PA6;	// Синие светодиоды подсветки

const uint8_t BUZZER = PIN_PA5;	// Пьезоизлучатель

const uint8_t A_1_2 = PIN_PB0;	// Аноды 1 и 2 светодиодов, ниже также используется (PINB & _BV(0))
const uint8_t A_3_4 = PIN_PA0;	// Аноды 3 и 4 светодиодов, ниже также используется (PINA & _BV(0))
const uint8_t C_1_3 = PIN_PA3;	// Катоды 1 и 3 светодиодов
const uint8_t C_2_4 = PIN_PB1;	// Катоды 2 и 4 светодиодов

const uint8_t RELAY = PIN_PA4;	// Реле, ниже также используется PORTA ^= _BV(PA4);

const uint8_t NTC = PIN_PA1; // Аналоговый вход датчика температуры, ниже также используется A1



const uint8_t arrayTemp[5] = {40, 60, 70, 80, 90};	// Массив температур для просмотра текущей
const int16_t chooseTemp[5] = {550, 440, 390, 331, 278};	// Соответствие выбранной температуры АЦП
const uint8_t chooseRGB[5][3] = {{255,192,0}, {0,255,255}, {0,0,255}, {96,0,255}, {255,0,192}};	// Цвет свечения подсветки при выборе температуры

const uint8_t arrTimeEff[] = {255, 128, 64, 32, 40, 20, 10, 5, 255, 128, 64, 32, 68, 34, 17, 9};	// Массив для счетчика обновления подсветки (скорость эффектов 1-16, меньше - быстрее)





volatile uint32_t timeCount = 0;	// Счетчик времени (подобие millis) ~4 = 1 мс.

uint32_t prevCount = 0;		// Счетчик для проверки кнопок

uint8_t keyPress = 0;		// Нажатие и удержание кнопок
uint8_t numKeyPress = 0;	// Какая кнопка была нажата (сохранение для обработки при отпускании)
uint32_t keyPressCount = 0;	// Счетчик для нажатия кнопок


uint8_t kettleMode = 0;		// Режим работы нагревателя(0 - выключен, 1-5 температурные режимы, 99 - неполное кипячение, 100 - кипячение)
uint8_t boilMode = 0;		// Режим работы таймера выключения (0 - выкл., 1 - 16 сек., 2 - 8 сек., 3 - 4 сек.)
uint32_t boilCount = 0;		// Счетчик для таймера выключения

bool chooseTimer = false;	// Включение таймера отсчета после выбора температуры кнопкой
uint8_t chooseMode = 0;		// Выбранная температура (1-5)
uint32_t chooseCount = 0;	// Счетчик выбора температуры

uint32_t enabledCount = 0;	// Время с момента включения нагревателя (для отключения, если долго не выключается сам  по температуре)

int16_t lastMinNTC = 1023;	// Последняя максимальная температура (минимальное число с АЦП)
uint32_t lastMinCount = 0;	// Время последнего обновления температуры (для отключения если долго не изменяется)

int16_t analogNTC = 512;	// АЦП датчика температуры


uint8_t ledsMode = 0;		// Режим работы подсветки (1-16 - различные режимы, 0 или 17 - цвет в соответствии с текущей температурой)
uint8_t ledsColor = 0;		// Цвет
uint8_t ledsBright = 0;		// Яркость
bool ledsDir = true;		// Изменение яркости (true - увеличивается, false - уменьшается)
uint32_t ledsCount = 0;		// Счетчик для обновления подсветки

uint8_t effectMode = 0; 	// Выбранный режим работы подсветки в режиме просмотра
uint32_t effectCount = 0;	// Счетчик для выключения режима просмотра при длительном бездействии





ISR(TIM0_OVF_vect)	// Прерывание по переполнению таймера 0
{
	timeCount++;	// Увеличение счетчика

	if(kettleMode)	// Если реле включено, то эмулировать ШИМ на пине для реле (частота ШИМ ~2кГц, при тактовой частоте микроконтроллера 8 МГц)
	{
		PORTA ^= _BV(PA4);	// digitalWrite(RELAY,  !digitalRead(RELAY));
	}
}


void showRGB(uint8_t r, uint8_t g, uint8_t b)	// Цвет в формате RGB отдельными параметрами
{
	analogWrite(LED_R, 255-r);
	analogWrite(LED_G, 255-g);
	analogWrite(LED_B, 255-b);
}


void showRGB(uint8_t rgb[3])	// Цвет в формате RGB массив
{
	showRGB(rgb[0], rgb[1], rgb[2]);
}


void showHV(uint8_t hue, uint8_t value)	// Цветовое колесо 0-255 и яркость
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
		temp |= ((analogRead(A1)^ledsColor) & 0b11) << i;
	}
	
	return temp;
}


void effectInit()	// Инициализация при включении/смене эффекта
{
	ledsColor = rand255();
	ledsBright = 0;
	ledsDir = true;
}


void effectOff()	// Выключение эффекта
{
	effectMode = 0;
	showRGB(0, 0, 0);
}


uint8_t getColorTemp()	// Преобразование из температуры 40-100°C в цвет от желтого до красного
{
	if(analogNTC < 235)
		return 0;
	else if(analogNTC > 570)
		return 32;
	else
		return ((int16_t)570 - analogNTC) * 2 / 3 + 32;
}


void showColorTemp()	// Показать температуру цветом подсветки
{
	//showHV(constrain(map(analogNTC, 221, 545, 255, 43), 43, 255), 255);
	showHV(getColorTemp(), 255);
}


void showEffectMode(uint8_t mode = ledsMode)	// Эффекты подсветки
{
	if(mode > 0 && mode < 17)
	{		
		if(timeCount - ledsCount >= arrTimeEff[mode-1])
		{
			ledsCount = timeCount;
			
			if(mode <= 4)	// Плавная смена цвета
			{
				ledsColor++;
				
				showHV(ledsColor, 255);
			}
			else
			{
				if(ledsDir)
				{
					ledsBright++;
					
					if(ledsBright == (mode <= 8 ? 255 : (mode <= 12 ? 16 : 150)))
					{
						ledsDir = false;
					}
				}
				else
				{
					ledsBright--;
					
					if(ledsBright == 0)
					{
						ledsDir = true;
						
						if(mode <= 12)
							ledsColor += 41 + rand255()%184;	// Генерация случайного цвета
					}
				}

				if(mode <= 8)	// Вспышки случайного цвета
					showHV(ledsColor, ledsBright);
				else if(mode <= 12)	// Резкая смена на случайный цвет
					showHV(ledsColor, 255);
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
			digitalWrite(A_3_4, LOW); pinMode(A_3_4, OUTPUT);
			pinMode(C_2_4, INPUT);
			digitalWrite(A_1_2, HIGH); pinMode(A_1_2, OUTPUT);
			pinMode(C_1_3, OUTPUT);
			break;
			
		case 2:
			digitalWrite(A_3_4, LOW); pinMode(A_3_4, OUTPUT);
			digitalWrite(A_1_2, HIGH); pinMode(A_1_2, OUTPUT);
			pinMode(C_1_3, OUTPUT);
			pinMode(C_2_4, OUTPUT);
			break;
			
		case 3:
			digitalWrite(A_3_4, LOW); pinMode(A_3_4, OUTPUT);
			pinMode(C_1_3, INPUT);
			digitalWrite(A_1_2, HIGH); pinMode(A_1_2, OUTPUT);
			pinMode(C_2_4, OUTPUT);
			break;
			
		case 4:
			digitalWrite(A_1_2, LOW); pinMode(A_1_2, OUTPUT);
			pinMode(C_2_4, INPUT);
			digitalWrite(A_3_4, HIGH); pinMode(A_3_4, OUTPUT);
			pinMode(C_1_3, OUTPUT);
			break;
			
		case 5:
			digitalWrite(A_1_2, LOW); pinMode(A_1_2, OUTPUT);
			pinMode(C_1_3, INPUT);
			digitalWrite(A_3_4, HIGH); pinMode(A_3_4, OUTPUT);
			pinMode(C_2_4, OUTPUT);
			break;

		default:
			pinMode(A_1_2, INPUT_PULLUP);
			pinMode(A_3_4, INPUT_PULLUP);
			pinMode(C_1_3, INPUT);
			pinMode(C_2_4, INPUT);
	}
}


void toneBuzzer(uint32_t frequency, uint32_t duration)	// Проиграть звук определенной частоты и длительности
{
	if(frequency)
	{
		uint32_t durDelay = 1000000UL / frequency / 2 - 6;
		uint32_t numCycles = 1000UL * duration / (durDelay * 2);
		
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
	enabledCount = timeCount;
	lastMinCount = timeCount;
	lastMinNTC = 1023;

	showRGB(0,0,0);
	showLedOn(0);
	digitalWrite(RELAY, HIGH);
	delay(70);
	kettleMode = numMode;
	effectInit();
}


void kettleOff()	// Выключение реле и сброс различных параметров
{
	kettleMode = 0;
	boilMode = 0;
	digitalWrite(RELAY, LOW);
	showRGB(0,0,0);
	chooseTimer = false;
	chooseMode = 0;
	//showLedOn();
}


void playCompleteAndDelay()	// Мелодия выключения чайника
{
	toneBuzzer(2048, 80);
	delay(1000);
}



void setup()
{  
	wdt_enable(WDTO_8S);	// Включение сторожевого таймера (WatchDog), 8 секунд
	
	digitalWrite(RELAY, LOW);
	pinMode(RELAY, OUTPUT);

	digitalWrite(BUZZER, HIGH);
	pinMode(BUZZER, OUTPUT);

	pinMode(NTC, INPUT);

	digitalWrite(LED_R, HIGH);
	digitalWrite(LED_G, HIGH);
	digitalWrite(LED_B, HIGH);

	pinMode(LED_R, OUTPUT);
	pinMode(LED_G, OUTPUT);
	pinMode(LED_B, OUTPUT);


	ledsMode = eeprom_read_byte((uint8_t*)1);	// Считывание режима работы подсветки из ячейки 1 EEPROM

	// Настройка двух таймеров, изменение делителей и режима работы ШИМ, чтобы сделать одинаковые частоты на 3 ШИМ выходах для RGB, хотя это и необязательно
	TCCR0B = _BV(CS01);
	TCNT0 = 0;
	TIMSK0 = _BV(TOIE0);	// Включение прерывания таймера 0

	TCCR1A = _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS11);
	TCNT1 = 0;


	/*
	pinMode(A_1_2, INPUT_PULLUP);
	pinMode(A_3_4, INPUT_PULLUP);
	pinMode(C_1_3, INPUT);
	pinMode(C_2_4, INPUT);
	*/
	showLedOn();
	
	toneBuzzer(1024, 50);

	// Вспышка случайного цвета при постановке чайника на подставку
	//uint8_t colorFlash = rand255(); // -312 байт экономия без random()
	analogNTC = analogRead(A1);
	uint8_t colorFlash = getColorTemp();	// Вспышка цветом в соответствии с температурой
	
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
		//delay(5-i/90*2);	// Более плавная вспышка (при низкой яркости - медленнее, при высокой - быстрее, так как при высокой яркости изменение почти незаметно)
		delay(2);
		
		if(!(PINA & _BV(0)) || !(PINB & _BV(0)))	// Чтобы не ждать окончания вспышки, можно включать чайник кнопками
			break;
	}

	//showLedOn();
}


void loop()
{
	wdt_reset();	// Сброс сторожевого таймера


	analogNTC = analogRead(A1);

	if(analogNTC <= 200)  // Если температура выше 105°C, то моргать подсветкой красного цвета и издавать прерывистый сигнал
	{
		kettleOff();

		showRGB(255,0,0);
		toneBuzzer(2048, 500);
		showRGB(0,0,0);
		delay(500);
		return;
	}
	else if(analogNTC > 670)  // Обрыв датчика температуры, 695 когда один резистор 47,5кОм без NTC; моргать подсветкой красного цвета
	{
		kettleOff();

		showRGB(255,0,0);
		delay(500);
		showRGB(0,0,0);
		delay(500);
		return;
	}
	else if(kettleMode)	// Нагреватель включен
	{
		showEffectMode();
		
		if(analogNTC < lastMinNTC)	// Обновление максимальной температуры
		{
			lastMinCount = timeCount;
			lastMinNTC = analogNTC;
		}
		
		if(timeCount - lastMinCount >= 225000)	// Температура не изменяется больше 1 минуты, выключить чайник и воспроизвести сигнал 2 раза
		{
			kettleOff();

			toneBuzzer(2048, 200);
			delay(500);
			toneBuzzer(2048, 200);
		}
		else if(timeCount - enabledCount >= 2250000)  // Включен более 10 минут, выключить чайник и воспроизвести сигнал 3 раза
		{
			kettleOff();

			toneBuzzer(2048, 200);
			delay(500);
			toneBuzzer(2048, 200);
			delay(500);
			toneBuzzer(2048, 200);
		}
		else if(analogNTC < 235)  // Температура 96-97°C, начать отсчет таймера
		{
			if(boilMode == 0)
			{
				boilCount = timeCount;
				boilMode = (kettleMode == 99 ? 3 : 1);
			}
		}
		else if(kettleMode <= 5)  // Если включен режим подогрева до нужной температуры
		{
			if(analogNTC < chooseTemp[kettleMode-1])
			{
				kettleOff();
				playCompleteAndDelay();
			}
		}

		if(boilMode)	// Если начался отсчет времени таймера выключения
		{
			if(timeCount - boilCount >= (boilMode == 1 ? 64000 : (boilMode == 2 ? 32000 : 16000)))	// ~16 сек., ~8 сек., ~4 сек.
			{
				kettleOff();
				playCompleteAndDelay();
			}
		}
	}
	else if(effectMode)	// Если включен режим просмотра эффекта
	{
		if(timeCount - effectCount < 4500000)	// ~20 минут
		{
			showEffectMode(effectMode);
		}
		else
		{
			effectOff();
		}
	}

	if(timeCount - prevCount >= 20)	// Проверка нажатия кнопок
	{
		prevCount = timeCount;

		pinMode(A_1_2, INPUT_PULLUP);
		pinMode(A_3_4, INPUT_PULLUP);
		pinMode(C_1_3, INPUT);
		pinMode(C_2_4, INPUT);

		uint8_t nPress = 0;

		if(!(PINA & _BV(0)))
			nPress = 1;
		else if(!(PINB & _BV(0)))
			nPress = 2;

		showLedOn();

		if(nPress)
		{
			if(timeCount-keyPressCount >= (keyPress ? (keyPress > 1 ? 12000 : 3000) : 200))	// Длительное удержание ~3 сек., 0,75 сек. и антидребезг 50 мс.
			{
				keyPressCount = timeCount;
				numKeyPress = nPress;

				toneBuzzer(512, 50);
				
				switch(nPress)
				{
					case 1:	// Первая кнопка
						if(!keyPress)	// Короткое нажатие
						{
							if(!effectMode)	// Режим просмотра эффектов выключен
							{
								if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
								{
									kettleOn(100);	// Запустить кипячение
									
									if(analogNTC < 227)  // Температура 99°C, начать отсчет 8 секунд
									{
										//if(boilMode == 0)
										{
											boilCount = timeCount;
											boilMode = 2;
										}
									}
								}
								else
								{
									kettleOff();	// Выключить чайник
								}
							}
							else	// Режим просмотра эффектов
							{
								effectOff();	// Выйти из режима просмотра эффектов
							}
						}
						else if(keyPress == 1)	// Недолгое удержание
						{
							if(kettleMode == 100)	// Режим кипячения включен
							{
								kettleMode = 99;	// Включить режим неполного кипячения
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

									int16_t realTemp = 139 - (analogNTC*2+6)/11;	// и количеством вспышек светодиодов, 1 вспышка - 5 градусов
									for(int8_t i=4; i>=0; i--)
									{
										if(realTemp >= arrayTemp[i])
										{
											//for(int8_t j=0, jj=(realTemp-arrayTemp[i]+3)/5+1; j<jj; j++)
											for(int8_t j=0, jj=(realTemp-arrayTemp[i])/5+1; j<jj; j++)
											{
												showLedOn(i+1);
												delay(j == 0 ? 1000 : 500);
												showLedOn(0);
												delay(500);
											}
											break;
										}
									}
									delay(1000);
									
									showRGB(0,0,0);
								}
								else if(chooseMode)	// Режим подогрева
								{
									eeprom_update_byte((uint8_t*)0, chooseMode);	// Сохранить выбранную температуру в ячейку 0 EEPROM
									showLedOn(0);	// Погасить светодиоды на время для индикации сохранения
									delay(200);
								}
							}
							else if(keyPress == 2)	// Удержание 3 сек.
							{
								if(kettleMode == 0 && chooseMode == 0)	// Режим ожидания
								{
									effectMode = 1;	// Включить режим просмотра эффектов
									effectCount = timeCount;
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
								showRGB(0,0,0);
								delay(200);
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
			if(timeCount-keyPressCount >= 200)	// Антидребезг 50 мс
			{
				if(numKeyPress == 2)	// Была нажата кнопка 2
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
								showRGB((uint8_t*)chooseRGB[chooseMode-1]);
								chooseCount = timeCount;
								chooseTimer = true;
							}
						}
						else	// Режим просмотра эффектов
						{
							effectMode = effectMode%17+1;	// Смена эффектов по кругу от 1 до 17
							effectCount = timeCount;
							effectInit();
						}
					}
				}
				
				keyPressCount = timeCount;
				keyPress = 0;
			}
		}
	}

	if(chooseTimer)	// Если включен таймер выбора температуры
	{
		if(timeCount-chooseCount >= 12000) // более 3 сек.
		{
			chooseTimer = false;	// Таймер отключить
			
			if(chooseMode && analogNTC > chooseTemp[chooseMode-1]+25)	// Если температура меньше выбранной
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
