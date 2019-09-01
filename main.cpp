#include <iostream>
#include <SFML\Graphics.hpp>
#include <SFML\System.hpp>
#include <windows.h>
#include <vector>

#pragma region Functions
#define frameRate 60
#define framesPerSecond 1000 / frameRate
#define WIDTH 1280.0f
#define HEIGHT 720.0f
#define PI 3.1415926535897932
#define RAD2DEG 180.0f / PI
#define DEG2RAD PI / 180.0f

typedef struct Vector
{
	float x, y, width, height;
};

typedef struct InputStruct
{
	bool up, down, left, right, fire;
};

sf::RenderWindow* window;
sf::Texture playerTexture;
sf::Texture meteorTexture[3];
sf::Sprite playerSprite;
InputStruct playerInput;

double lerp(double start, double end, double speed)
{
	return (end - start) * speed + start;
}

float clamp(float _value, float _min, float _max)
{
	return std::max(std::min(_value, _max), _min);
}

float reverseLerp(float start, float end, float speed, float reference)
{
	return ((end - start) / reference * speed) + start;
}

sf::Vector2f reverseLerp(sf::Vector2f start, sf::Vector2f end, sf::Vector2f speed, sf::Vector2f reference)
{
	return sf::Vector2f(reverseLerp(start.x, end.x, speed.x, reference.x), reverseLerp(start.y, end.y, speed.y, reference.y));
}

sf::Vector2f reverseLerp(sf::Vector2f start, float end, float speed, float reference)
{
	return reverseLerp(start, sf::Vector2f(end, end), sf::Vector2f(speed, speed), sf::Vector2f(reference, reference));
}

bool collide(Vector self, Vector other)
{
	return (
		(self.x + self.width > other.x && self.x < other.x + other.width) &&
		(self.y + self.height > other.y && self.y < other.y + other.height)
		);
}

double random()
{
	return (double)rand() / RAND_MAX;
}

double random(float end)
{
	return random() * end;
}

double random(float start, float end)
{
	return random(end - start) + start;
}

int sign(float x)
{
	if (x == 0) return 0;
	return x / std::abs(x);
}

#pragma endregion

#define SHOTSPEED 4.0f
#define PLAYERREVIVECOOLDOWN frameRate * 3

int points;
int pointsPerKill = 100;
int pointsPerLives = 10000;
int lives = 2;

int playerReviveCooldown = 0;
bool playerAlive = true;

sf::Text pointsText;

#pragma region Meteors
class Meteor
{
public:
	float direction;
	float rotation;
	float rotationSpeed;
	float speed;
	int type;
	bool inWindow = false;
	sf::Vector2f position;
	sf::Sprite sprite;

	Meteor(float x, float y, float direction, int type)
	{
		this->position = sf::Vector2f(x, y);
		this->direction = direction;
		this->rotation = random(PI * 2);
		this->rotationSpeed = random(-0.05f, 0.05f);
		this->speed = random(0.1f, 1.1f);

		sf::Texture* texture = &meteorTexture[type];
		this->type = type;

		sprite.setTexture(*texture);

		sf::Vector2u _size = texture->getSize();
		sprite.setOrigin(sf::Vector2f(_size.x / 2, _size.y / 2));

		sprite.setScale(sf::Vector2f(2, 2));
	}
};

std::vector<Meteor> meteors;

void createMeteor(int type)
{
	int _startpoint = rand() % 4;

	float _x, _y, _dir;
	float offset = 60.0f;
	switch (_startpoint) {
		case 0:
			_x = WIDTH + 30;
			_y = random(offset, HEIGHT - offset);
			_dir = random(135, 225);
			break;
		case 1:
			_x = random(offset, WIDTH - offset);
			_y = -30;
			_dir = random(225, 315);
			break;
		case 2:
			_x = -30;
			_y = random(offset, HEIGHT - offset);
			_dir = random(-45, 45);
			break;
		case 3:
			_x = random(offset, WIDTH - offset);
			_y = HEIGHT + 30;
			_dir = random(45, 135);
			break;
	}

	meteors.push_back(Meteor(_x, _y, _dir, type));
}

void destroyMeteor(int index, bool shooted)
{
	if (meteors[index].type > 0)
		for (int i = 0; i < 3; i++) {
			Meteor newMeteor = Meteor(meteors[index].position.x, meteors[index].position.y, random(PI * 2), meteors[index].type - 1);
			newMeteor.speed *= 2;
			meteors.push_back(newMeteor);
		}
	meteors.erase(meteors.begin() + index);
	if (!shooted) return;

	points += pointsPerKill;
	if (points >= pointsPerLives) {
		lives++;
		points -= pointsPerLives;
	}
}

void processMeteors()
{
	for (int i = 0; i < meteors.size(); i++) {
		Meteor* _meteor = &meteors[i];

		_meteor->position += sf::Vector2f(
			cos(_meteor->direction) * _meteor->speed,
			sin(_meteor->direction) * _meteor->speed
		);

		_meteor->rotation += _meteor->rotationSpeed;

		sf::Vector2u __size = _meteor->sprite.getTexture()->getSize();
		if (
			!(_meteor->position.x > __size.x / 2 && _meteor->position.y > __size.y / 2 &&
			  _meteor->position.x < WIDTH - __size.x / 2 && _meteor->position.y < HEIGHT - __size.y / 2)
			) {

			if (_meteor->inWindow) destroyMeteor(i,false);
		} else
			_meteor->inWindow = true;
	}
}

void drawMeteors()
{
	for (int i = 0; i < meteors.size(); i++) {
		Meteor* _meteor = &meteors[i];

		_meteor->sprite.setPosition(_meteor->position);
		_meteor->sprite.setRotation(_meteor->rotation);
		window->draw(_meteor->sprite);
	}
}

#pragma endregion

#pragma region Shots
sf::RectangleShape shotShape;
class Shot
{
public:
	sf::Vector2f position;
	float direction;
	Shot() {}
	Shot(sf::Vector2f position, float direction)
	{
		this->position = position;
		this->direction = direction;
	}
};
std::vector<Shot> shots;

void processShots()
{
	for (int i = 0; i < shots.size(); i++) {
		Shot* _shot = &shots[i];
		_shot->position += sf::Vector2f(cos(_shot->direction) * SHOTSPEED, sin(_shot->direction) * SHOTSPEED);

		if (_shot->position.x != clamp(_shot->position.x, 0, WIDTH) || _shot->position.y != clamp(_shot->position.y, 0, HEIGHT)) {
			shots.erase(shots.begin() + i);
			i--;
			continue;
		}

		for (int j = 0; j < meteors.size(); j++) {
			Meteor* _meteor = &meteors[j];

			Vector _me = {_shot->position.x,_shot->position.y,4,4};
			sf::Vector2u __size = _meteor->sprite.getTexture()->getSize();
			Vector _other = {_meteor->position.x - __size.x,_meteor->position.y - __size.y,__size.x * 2,__size.y * 2};

			if (collide(_me, _other)) {
				destroyMeteor(j,true);
				shots.erase(shots.begin() + i);
				i--;
				break;
			}
		}
	}
}

void drawShots()
{
	for (int i = 0; i < shots.size(); i++) {
		Shot* _shot = &shots[i];

		shotShape.setPosition(_shot->position);
		window->draw(shotShape);
	}
}

#pragma endregion

#pragma region Player
class Player
{
public:
	float maxSpeed = 1.85f;
	float speed = 0.05f;
	float rotation = 0;
	float rotationSpeed = 0.05f;
	float grv = 0.02f;
	int firedelay = 0;
	int firedelayDefault = 10;
	int invTimer = 0;
	sf::Vector2f position;
	sf::Vector2f velocity;
	sf::Sprite sprite;

	Player() {}
	Player(float x, float y, sf::Texture* texture)
	{
		position = sf::Vector2f(x, y);
		sprite.setTexture(*texture);
		sprite.setScale(sf::Vector2f(2, 2));
		sprite.setOrigin(sf::Vector2f(8, 8));

		rotation = PI / 2;
	}

	void Step()
	{
		int dir = (int)playerInput.right - (int)playerInput.left;

		rotation += dir * rotationSpeed;

		if (playerInput.up) {
			velocity.x = clamp(velocity.x + (cos(rotation) * speed), -maxSpeed, maxSpeed);
			velocity.y = clamp(velocity.y + (sin(rotation) * speed), -maxSpeed, maxSpeed);
		}

		velocity = reverseLerp(velocity, 0, grv, maxSpeed);

		position += velocity;

		position.x = clamp(position.x, 0, WIDTH);
		position.y = clamp(position.y, 0, HEIGHT);

		firedelay -= (int)(firedelay > 0);
		if (playerInput.fire && firedelay == 0) {
			float _dir = rotation + random(-0.01f, 0.01f);

			float _x = position.x + cos(rotation) * 8,
				_y = position.y + sin(rotation) * 8;
			shots.push_back(Shot(sf::Vector2f(_x, _y), _dir));
			firedelay = firedelayDefault;
		}

		invTimer -= (int)(invTimer > 0);
		if (invTimer == 0)
		for (int i = 0; i < meteors.size(); i++) {
			Meteor* _meteor = &meteors[i];

			sf::Vector2u __size = sprite.getTexture()->getSize();
			Vector _me = {position.x - __size.x / 2,position.y - __size.y / 2,__size.x,__size.y};

			__size = _meteor->sprite.getTexture()->getSize();
			Vector _other = {_meteor->position.x - __size.x,_meteor->position.y - __size.y,__size.x * 2,__size.y * 2};

			if (collide(_me, _other)) {
				if (lives > 0) {
					playerAlive = false;
					playerReviveCooldown = PLAYERREVIVECOOLDOWN;
					lives--;

					velocity = sf::Vector2f(0,0);
					invTimer = frameRate * 3;
				} else {
					window->close();
				}
			}
		}
	}
	void Draw()
	{
		sprite.setPosition(position);
		sprite.setRotation((double)rotation * RAD2DEG);
		window->draw(sprite);
	}
};
#pragma endregion

#pragma region Main
Player player;
void PreStep()
{
	playerInput = {
		sf::Keyboard::isKeyPressed(sf::Keyboard::Up),
		sf::Keyboard::isKeyPressed(sf::Keyboard::Down),
		sf::Keyboard::isKeyPressed(sf::Keyboard::Left),
		sf::Keyboard::isKeyPressed(sf::Keyboard::Right),
		sf::Keyboard::isKeyPressed(sf::Keyboard::Z)
	};
}

void Step()
{
	playerReviveCooldown -= (int)(playerReviveCooldown > -1);

	if (playerReviveCooldown == 0) {
		player.position = sf::Vector2f(WIDTH/2,HEIGHT/2);
		playerAlive = true;
	}

	if (random() > 0.97 - lerp(0,0.1,(double)lives / 10))
		createMeteor((rand() % 2) + 1);

	if (playerAlive) player.Step();
	processMeteors();
	processShots();
}

void Draw()
{
	if (playerAlive && (player.invTimer % (frameRate / 2)) < frameRate / 4) player.Draw();
	drawMeteors();
	drawShots();
	
	pointsText.setString(std::to_string(points));
	window->draw(pointsText);

	int tam = playerTexture.getSize().x;
	int offset = 2;
	int margin = 15;

	for (int i = 0; i < lives; i++) {
		playerSprite.setPosition(sf::Vector2f(margin + tam * i + offset * i, 60));
		window->draw(playerSprite);
	}
}

int main()
{
	srand(time(NULL));
	ShowWindow(GetConsoleWindow(), SW_HIDE); // Hide console window
	sf::RenderWindow wind(sf::VideoMode(1280, 720), "Asteroid", sf::Style::Close | sf::Style::Titlebar); // Create game window
	window = &wind;

	// Load Textures
	playerTexture.loadFromFile("player.png");
	meteorTexture[0].loadFromFile("meteor_small.png");
	meteorTexture[1].loadFromFile("meteor_medium.png");
	meteorTexture[2].loadFromFile("meteor_big.png");
	sf::Font gamefont;
	gamefont.loadFromFile("LucidaTypewriterBold.ttf");
	
	// Start the game
	player = Player(WIDTH / 2, HEIGHT / 2, &playerTexture);
	shotShape.setSize(sf::Vector2f(4, 4));
	shotShape.setFillColor(sf::Color(255, 255, 255, 255));
	shotShape.setOutlineColor(sf::Color(0, 0, 0, 0));
	shotShape.setOrigin(sf::Vector2f(2, 2));

	playerSprite.setTexture(playerTexture);
	playerSprite.setRotation(270);
	playerSprite.setOrigin(sf::Vector2f(8, 8));
	//playerSprite.setScale(sf::Vector2f(1.5f, 1.5f));

	pointsText.setFont(gamefont);
	pointsText.setFillColor(sf::Color(255, 255, 255, 255));
	pointsText.setOutlineColor(sf::Color());
	pointsText.setOutlineThickness(2);
	pointsText.setPosition(sf::Vector2f(10,10));
	pointsText.setScale(sf::Vector2f(1,1));

	while (wind.isOpen()) {
		sf::Event evnt;
		while (wind.pollEvent(evnt)) {
			if (evnt.type == sf::Event::Closed)
				wind.close();
		}

		PreStep();
		Step();

		wind.clear();
		Draw();
		wind.display();

		sf::sleep(sf::milliseconds(framesPerSecond));
	}

	return 0;
}
#pragma endregion
