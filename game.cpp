#include "game.h"
#include "block.h"
#include "lifedisplay.h"
#include "qapplication.h"
#include "soundplayer.h"
#include "trap.h"
#include "settingsmanager.h"
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
#include <QKeyEvent>
#include <QBrush>
#include <QGraphicsView>
#include "coin.h"
#include <QGraphicsProxyWidget>
#include "enemy.h"
#include "healthpotion.h"
#include <soundplayer.h>


Game::Game() {

    // Initialize game
    init();

}


// Logic
void Game::init() {

    // Initialize sound player
    SP.init();

    // Play starting menu sound
    SP.startingMenuMusic->play();

    // Initialize game starting menu
    startingMenu = new StartingMenu();
    startingMenu->show();

    connect(startingMenu, &StartingMenu::startGameSignal, this, &Game::startCurrentLevel);
    connect(startingMenu, &StartingMenu::exitGameSignal, this, &Game::close);


    // Initialize state and level
    state = new State();

    // Initialize Scene
    scene = new QGraphicsScene();
    setScene(scene);

    // View width and height
    setFixedSize(SM.settings->value("window/width").toInt(), SM.settings->value("window/height").toInt());

    // Ensuring view can receive focus
    setFocusPolicy(Qt::StrongFocus);

    // Initialize Player
    player = new Player();
    scene->addItem(player);
    connect(player, &Player::playerPositionChanged, this, &Game::handlePlayerMovement);

    // End flag
    endFlag = new QGraphicsPixmapItem(QPixmap(SM.settings->value("scene/endFlag").toString()).scaled(75, 115));
    scene->addItem(endFlag);

    // Coins displayer
    coinsDisplayer = new CoinsDisplay();
    coinsDisplayer->setPos(
        SM.settings->value("window/coinsDisplayerXOffset").toInt(),
        SM.settings->value("window/coinsDisplayerYOffset").toInt()
        );
    scene->addItem(coinsDisplayer);

    // Life displayer
    lifeDisplay = new LifeDisplay(state);
    lifeDisplayer = scene->addWidget(lifeDisplay);
    lifeDisplayer->setPos(
        SM.settings->value("window/lifeDisplayerXOffset").toInt(),
        SM.settings->value("window/lifeDisplayerYOffset").toInt()
        );

    emit state->stateChanged();

    // Connect state change signal
    connect(state, &State::stateChanged, this, &Game::handleStateChange);

}


void Game::startCurrentLevel() {

    // Initialize level
    if (level != nullptr)
        delete level;

    this->setWindowTitle("RUN - Level " + QString::number(state->getLevel()));

    if (startingMenu != nullptr)
        startingMenu->close();

    // Adjusting Sound
    SP.startingMenuMusic->stop();
    QTimer::singleShot(1000, [=](){
        SP.bgMusic->play();
    });

    this->show();

    level = new Level(LEVELS[state->getLevel() - 1]);

    // Scene width, height, and background
    scene->setSceneRect(0, 0, level->getSceneWidth(), level->getSceneHeight());
    scene->setBackgroundBrush(QBrush(QPixmap(level->getBackgroundPath()).scaled(width(), height())));

    // Player and Scene
    player->setFlag(QGraphicsItem::ItemIsFocusable);
    player->setFocus();
    player->startLevel();


    // Move with player
    // Disconnect from the playerPositionChanged signal and connect again
    disconnect(player, &Player::playerPositionChanged, this, &Game::handlePlayerMovement);
    connect(player, &Player::playerPositionChanged, this, &Game::handlePlayerMovement);
    moveWithPlayer();

    // Set scroll policies to hide scrollbars
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Create obstacles
    createMap();

    // Play Background Music
    SP.bgMusic->play();

    // End flag position
    endFlag->setPos(scene->width() - getEndOffset(), getGroundLevel() - endFlag->boundingRect().height());

}


void Game::handleNewLevel() {

    // Stop player movement
    player->clearFocus();
    player->setFlag(QGraphicsItem::ItemIsFocusable, false);

    // Give Player IDLE state
    player->deactivate();

    // Stop the background music
    SP.bgMusic->stop();

    // Play the win sound
    SP.levelWinSound->play();

    // play "victory" sound after 500 ms
    QTimer::singleShot(1000, [=](){
        SP.victorySound->play();
        if (state->getLevel() != LEVELS.size()) {
            state->setLevel(state->getLevel() + 1);
            startCurrentLevel();
        } else
            state->setIsGameOver(true);
    });

}

void Game::close() {
    QApplication::quit();
}

// Slots
void Game::handlePlayerMovement() {

    if (player->x() >= scene->width() - getEndOffset() - player->boundingRect().width()) {
        // Handle the transition to a new level
        handleNewLevel();
        // Disconnect player position change signal
        disconnect(player, &Player::playerPositionChanged, this, &Game::handlePlayerMovement);
        return;
    }

    // Move view with the player
    moveWithPlayer();

    // Move displayers with the player
    mapDisplayersToScene();
}

void Game::KeyPressEvent(QKeyEvent *event)
{
    // Prevent moving the view if it has focus
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Left) {
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void Game::handleStateChange() {
    coinsDisplayer->set(state->getCoins());
}

// Helpers
void Game::moveWithPlayer() {

    // Center the view on the player with boundaries
    qreal x = qBound(viewport()->width() / 2.0, player->x(), scene->width() - viewport()->width() / 2.0);
    centerOn(x, viewport()->height() / 2.0);

}

// TODO: Position them according to ground level
void Game::createMap() {

    // Enemy
    Enemy *enemy = new Enemy(600, this->getGroundLevel() - 125  , 2);
    elements.push_back(enemy);
    scene->addItem(enemy);



    // // Create coins
    QString path = SM.settings->value("coin/spriteSheet/1").toString();

    // // // Create pixmap
    for ( int i = 0 ; i < 40 ; i++){
        Coin *coin = new Coin(800+ i*200, this->getGroundLevel() -200 , 2 , 1 , path);
        elements.push_back(coin);
        scene->addItem(coin);
    }


    QString path2 = SM.settings->value("spikes/1").toString();
    for ( int i = 0 ; i < 40 ; i++){
        Trap *trap = new Trap(800+ i*200, this->getGroundLevel() - 11 , path2 ,2 );
        elements.push_back(trap);
        scene->addItem(trap);
    }

    QString path3 = SM.settings->value("blocks/2").toString();

    for ( int i = 0 ; i < 20 ; i++){
        Block *block = new Block(700+ i*150, 385 ,   path3 , 1 );
        elements.push_back(block);
        scene->addItem(block);
        // Put Potions Randomly
        int random = RandomNumber(0, 1);
        if (random > 0){
            auto *potion = new HealthPotion(700 + i*150  , 300  , 1.5 , SM.settings->value("potion/health/path").toString());
            elements.push_back(potion);
            scene->addItem(potion);
        }
    }

    QString path4 = SM.settings->value("blocks/3").toString();

    for ( int i = 0 ; i < 14 ; i++){
        Block *block = new Block(2800+ i*150, 300 - i*10 ,   path4 , 1);
        elements.push_back(block);
        scene->addItem(block);
        // Put Potions Randomly

    }

}

void Game::mapDisplayersToScene() {
    QPointF coinsTopLeft = mapToScene(
        SM.settings->value("window/coinsDisplayerXOffset").toInt(),
        SM.settings->value("window/coinsDisplayerYOffset").toInt()
        );
    coinsDisplayer->setPos(coinsTopLeft.x(), coinsTopLeft.y());


    QPointF lifeTopLeft = mapToScene(
        SM.settings->value("window/lifeDisplayerXOffset").toInt(),
        SM.settings->value("window/lifeDisplayerYOffset").toInt()
        );
    lifeDisplayer->setPos(lifeTopLeft.x(), lifeTopLeft.y());
}


// Getters
float Game::getGroundLevel() {
    return level->getGroundLevel();
}

int Game::getStartOffset() {
    return level->getStartOffset();
}

int Game::getEndOffset() {
    return level->getEndOffset();
}

int Game::getSceneWidth() {
    return level->getSceneWidth();
}

int Game::getSceneHeight() {
    return level->getSceneHeight();
}


// Destructor
Game::~Game() {
    delete scene;
    delete player;
    delete state;
    delete endFlag;
    delete coinsDisplayer;
    delete lifeDisplayer;
    delete startingMenu;
    for (auto element : elements)
        delete element;

}
