#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <algorithm>
/* ===== FUNCTION DECLARATIONS ===== */

void display();
void reshape(int w, int h);
void keys(unsigned char k, int x, int y);
void update(int value);


/* ========================================================================
   SECTION 1: GLOBAL SETTINGS & VARIABLES
   ======================================================================== */

enum GameMode { MENU, PLAYING, PAUSED, GAMEOVER, COUNTDOWN };
GameMode mode = MENU;

long distanceScore = 0;
long coinScore = 0;
long highScore = 0;

long currentSegment = 0;
float roadOffset = 0.0f;

float scrollSpeed = 0.15f;
float baseScrollSpeed = 0.15f;
float maxScrollSpeed  = 0.45f;
float difficultyFactor = 0.0000025f;

const float segmentLength = 2.0f;
const int visibleSegments = 40;

float playerX = 0.0f;
float playerY = 0.5f;
int currentLane = 0;
float targetX = 0.0f;

const float laneWidth = 2.0f;
const float roadHalfWidth = 3.3f;
float laneSpeed = 0.3f;

bool isJumping = false;
float velY = 0.0f;
const float GRAVITY = 0.025f;
const float JUMP_FORCE = 0.35f;

float windmillAngle = 0.0f;
float countdownValue = 3.0f;

float dayCycle = 0.0f;

struct Car { long seg; int lane; };
struct Coin { long seg; int lane; bool collected; };

std::vector<Car> cars;
std::vector<Coin> coins;

/* ========================================================================
   HIGH SCORE SYSTEM
   ======================================================================== */

void loadHighScore() {
    FILE* f = fopen("highscore.txt", "r");
    if (f) { fscanf(f, "%ld", &highScore); fclose(f); }
}

void saveHighScore() {
    if (distanceScore > highScore) {
        highScore = distanceScore;
        FILE* f = fopen("highscore.txt", "w");
        if (f) { fprintf(f, "%ld", highScore); fclose(f); }
    }
}

/* ========================================================================
   CUSTOM ALGORITHMS
   ======================================================================== */

void drawLineDDA(float x1, float y1, float z1,
                 float x2, float y2, float z2) {

    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;

    float steps = std::max(std::abs(dx),
                   std::max(std::abs(dy), std::abs(dz)));

    if (steps == 0) {
        glBegin(GL_POINTS);
        glVertex3f(x1, y1, z1);
        glEnd();
        return;
    }

    float xInc = dx / steps;
    float yInc = dy / steps;
    float zInc = dz / steps;

    float x = x1, y = y1, z = z1;

    glBegin(GL_POINTS);
    for (int i = 0; i <= steps; i++) {
        glVertex3f(x, y, z);
        x += xInc;
        y += yInc;
        z += zInc;
    }
    glEnd();
}

void drawMidpointCirclePoints(int radius, float pixelScale) {

    int x = 0;
    int y = radius;
    int p = 1 - radius;

    glBegin(GL_POINTS);

    while (x <= y) {

        float fx = x * pixelScale;
        float fy = y * pixelScale;

        glVertex3f( fx,  fy, 0);
        glVertex3f( fy,  fx, 0);
        glVertex3f(-fx,  fy, 0);
        glVertex3f(-fy,  fx, 0);
        glVertex3f(-fx, -fy, 0);
        glVertex3f(-fy, -fx, 0);
        glVertex3f( fx, -fy, 0);
        glVertex3f( fy, -fx, 0);

        x++;

        if (p < 0)
            p += 2 * x + 1;
        else {
            y--;
            p += 2 * x - 2 * y + 1;
        }
    }

    glEnd();
}

void drawFilledMidpointCircle(int radius, float scale) {
    for (int r = 0; r <= radius; r++)
        drawMidpointCirclePoints(r, scale);
}

/* ========================================================================
   TEXT HELPERS
   ======================================================================== */

void drawText(const char* s,
              float x, float y,
              float r, float g, float b) {

    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(r, g, b);
    glRasterPos2f(x, y);

    while (*s)
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *s++);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void drawCenteredText(const char* s,
                      float y,
                      float r, float g, float b) {

    int w = glutGet(GLUT_WINDOW_WIDTH);
    int stringWidth = 0;

    for (const char* p = s; *p; p++)
        stringWidth += glutBitmapWidth(GLUT_BITMAP_TIMES_ROMAN_24, *p);

    drawText(s,
             (w - stringWidth) / 2.0f,
             y, r, g, b);
}

/* ========================================================================
   BACKGROUND WITH DAY–NIGHT CYCLE
   ======================================================================== */

void drawCloud2D(float cx, float cy, float scale) {

    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glScalef(scale, scale * 0.6f, 1.0f);

    glColor4f(1.0f, 1.0f, 1.0f, 0.8f);

    glPushMatrix(); glTranslatef(-30, -10, 0);
    drawFilledMidpointCircle(25, 1.0f);
    glPopMatrix();

    glPushMatrix(); glTranslatef(0, 0, 0);
    drawFilledMidpointCircle(35, 1.0f);
    glPopMatrix();

    glPushMatrix(); glTranslatef(30, -10, 0);
    drawFilledMidpointCircle(25, 1.0f);
    glPopMatrix();

    glPushMatrix(); glTranslatef(15, 15, 0);
    drawFilledMidpointCircle(20, 1.0f);
    glPopMatrix();

    glPopMatrix();
}

void drawAttractiveBackground() {

    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float t = (sin(dayCycle) + 1.0f) * 0.5f;

    glBegin(GL_QUADS);

    glColor3f(0.05f + 0.3f*t,
              0.2f  + 0.4f*t,
              0.4f  + 0.4f*t);
    glVertex2f(0, h);
    glVertex2f(w, h);

    glColor3f(0.0f + 0.2f*t,
              0.1f + 0.3f*t,
              0.2f + 0.6f*t);
    glVertex2f(w, 0);
    glVertex2f(0, 0);

    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA);

    drawCloud2D(w * 0.2f, h * 0.85f, 1.5f);
    drawCloud2D(w * 0.75f, h * 0.75f, 2.0f);
    drawCloud2D(w * 0.45f, h * 0.9f, 1.2f);

    glPushMatrix();
    glTranslatef(w * 0.85f, h * 0.85f, 0);
    glColor3f(1.0f, 1.0f, 0.0f);
    drawFilledMidpointCircle(60, 1.0f);
    glPopMatrix();

    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}
/* ========================================================================
   3D MODELS AND SCENERY
   ======================================================================== */

void drawTree(float x, float z) {

    glColor3f(0.55f, 0.27f, 0.07f);

    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(-90, 1, 0, 0);

    GLUquadric* q = gluNewQuadric();
    gluCylinder(q, 0.25f, 0.25f, 1.5f, 8, 1);
    gluDeleteQuadric(q);

    glPopMatrix();

    glColor3f(0.1f, 0.7f, 0.1f);

    glPushMatrix();
    glTranslatef(x, 1.5f, z);
    glRotatef(-90, 1, 0, 0);
    glutSolidCone(1.0f, 2.3f, 10, 2);
    glPopMatrix();
}

/* ---------------------------------------------------------------------- */

void drawWindmill(float x, float z) {

    glColor3f(0.85f, 0.85f, 0.85f);

    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(-90, 1, 0, 0);

    GLUquadric* q = gluNewQuadric();
    gluCylinder(q, 0.5f, 0.25f, 5.0f, 12, 1);
    gluDeleteQuadric(q);

    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, 5.0f, z);
    glRotatef(windmillAngle, 0, 0, 1);

    glColor3f(0.8f, 0.0f, 0.0f);

    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glRotatef(90.0f * i, 0, 0, 1);
        glTranslatef(0, 1.5f, 0);
        glScalef(0.4f, 3.0f, 0.1f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    glPopMatrix();
}

/* ---------------------------------------------------------------------- */

void drawCartoonHouse(float x, float z) {

    glPushMatrix();
    glTranslatef(x, 0, z);
    glScalef(2.5f, 2.5f, 2.5f);
    glLineWidth(3.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
    drawLineDDA(-1, 0, 0, -1, 1, 0);
    drawLineDDA(1, 0, 0, 1, 1, 0);
    drawLineDDA(-1, 0, 0, 1, 0, 0);
    drawLineDDA(-1, 1, 0, 1, 1, 0);

    glColor3f(1.0f, 0.0f, 0.0f);
    drawLineDDA(-1.2f, 1, 0, 0, 1.8f, 0);
    drawLineDDA(1.2f, 1, 0, 0, 1.8f, 0);
    drawLineDDA(-1.2f, 1, 0, 1.2f, 1, 0);

    glColor3f(0.6f, 0.3f, 0.1f);
    drawLineDDA(-0.3f, 0, 0, -0.3f, 0.6f, 0);
    drawLineDDA(0.3f, 0, 0, 0.3f, 0.6f, 0);
    drawLineDDA(-0.3f, 0.6f, 0, 0.3f, 0.6f, 0);

    glColor3f(1.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0.15f, 0.3f, 0.05f);
    drawFilledMidpointCircle(3, 0.02f);
    glPopMatrix();

    glLineWidth(1.0f);
    glPopMatrix();
}

/* ---------------------------------------------------------------------- */

void drawCartoonCharacter(float x, float z) {

    glPushMatrix();
    glTranslatef(x, 0.7f, z);
    glScalef(1.5f, 1.5f, 1.5f);

    float bob = sin(distanceScore * 0.1f + x) * 0.05f;
    glTranslatef(0, bob, 0);

    glLineWidth(3.0f);
    glColor3f(1.0f, 0.8f, 0.6f);

    glPushMatrix();
    glTranslatef(0, 0.6f, 0);
    drawFilledMidpointCircle(8, 0.02f);
    glPopMatrix();

    glColor3f(0.0f, 0.8f, 0.0f);
    drawLineDDA(0, 0.6f, 0, 0, 0.2f, 0);

    float wave = std::abs(sin(distanceScore * 0.2f + z)) * 0.3f;

    drawLineDDA(0, 0.5f, 0, -0.3f, 0.4f, 0);
    drawLineDDA(0, 0.5f, 0,  0.3f, 0.4f + wave, 0);

    glColor3f(0.0f, 0.0f, 0.8f);
    drawLineDDA(0, 0.2f, 0, -0.2f, -0.5f, 0);
    drawLineDDA(0, 0.2f, 0,  0.2f, -0.5f, 0);

    glLineWidth(1.0f);
    glPopMatrix();
}

/* ---------------------------------------------------------------------- */
/* COIN WITH GLOW (no algorithm removed) */
/* ---------------------------------------------------------------------- */

void drawCoin(float x, float z) {

    glPushMatrix();
    glTranslatef(x, 0.9f, z);
    glRotatef((float)(distanceScore % 360) * 4.0f, 0, 1, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glColor4f(1.0f, 0.85f, 0.0f, 0.8f);

    drawFilledMidpointCircle(12, 0.02f);

    glPushMatrix();
    glTranslatef(0, 0, 0.05f);
    drawFilledMidpointCircle(12, 0.02f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 0, -0.05f);
    drawFilledMidpointCircle(12, 0.02f);
    glPopMatrix();

    glDisable(GL_BLEND);

    glPopMatrix();
}

/* ---------------------------------------------------------------------- */
/* ROBOT WITH SHADOW */
/* ---------------------------------------------------------------------- */

void drawRobot() {

    float runAnim =
        (mode == PLAYING)
        ? sin(distanceScore * 0.2f) * 30.0f
        : 0.0f;

    /* SHADOW */
    glDisable(GL_LIGHTING);
    glColor4f(0, 0, 0, 0.3f);
    glPushMatrix();
    glTranslatef(playerX, 0.01f, 0.0f);
    glScalef(1.0f, 0.1f, 1.2f);
    glutSolidSphere(0.4f, 12, 12);
    glPopMatrix();
    glEnable(GL_LIGHTING);

    /* BODY */
    glPushMatrix();
    glTranslatef(playerX, playerY + 0.6f, 0.0f);
    glScalef(0.65f, 0.65f, 0.65f);

    glColor3f(0.2f, 0.2f, 0.8f);
    glPushMatrix();
    glScalef(0.6f, 0.8f, 0.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.9f, 0.9f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glutSolidSphere(0.35f, 12, 12);
    glPopMatrix();

    glColor3f(0.6f, 0.6f, 0.6f);

    glPushMatrix();
    glTranslatef(0.4f, 0.2f, 0.0f);
    glRotatef(runAnim, 1, 0, 0);
    glTranslatef(0.0f, -0.35f, 0.0f);
    glScalef(0.15f, 0.7f, 0.15f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.4f, 0.2f, 0.0f);
    glRotatef(-runAnim, 1, 0, 0);
    glTranslatef(0.0f, -0.35f, 0.0f);
    glScalef(0.15f, 0.7f, 0.15f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.2f, 0.2f, 0.6f);

    glPushMatrix();
    glTranslatef(0.15f, -0.55f, 0.0f);
    glRotatef(-runAnim, 1, 0, 0);
    glTranslatef(0.0f, -0.45f, 0.0f);
    glScalef(0.2f, 0.9f, 0.2f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.15f, -0.55f, 0.0f);
    glRotatef(runAnim, 1, 0, 0);
    glTranslatef(0.0f, -0.45f, 0.0f);
    glScalef(0.2f, 0.9f, 0.2f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPopMatrix();
}
/* ========================================================================
   CAR MODEL
   ======================================================================== */

void drawCar(float x, float z) {

    glPushMatrix();
    glTranslatef(x, 0.35f, z);

    glColor3f(0.85f, 0.1f, 0.1f);
    glPushMatrix();
    glScalef(1.4f, 0.6f, 2.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.75f, 0.05f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, 0.45f, -0.2f);
    glScalef(1.0f, 0.45f, 1.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.1f, 0.1f, 0.1f);

    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            glPushMatrix();
            glTranslatef(0.55f * sx, -0.35f, 0.75f * sz);
            glutSolidTorus(0.05f, 0.13f, 10, 16);
            glPopMatrix();
        }

    glPopMatrix();
}

/* ========================================================================
   WORLD GENERATION
   ======================================================================== */

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float laneX(int lane) { return lane * laneWidth; }

float segmentZ(long seg) {
    return -((seg - currentSegment) * segmentLength + segmentLength * 0.5f);
}

void spawn(long seg) {

    uint32_t h = hash32((uint32_t)seg ^ 0xA53C9E11U);
    int safeLane = (int)(h % 3) - 1;

    for (int lane = -1; lane <= 1; lane++) {
        if (lane != safeLane &&
            (int)(hash32(h ^ (lane + 7) * 123u) % 100) < 15)
            cars.push_back({ seg, lane });
    }

    for (int lane = -1; lane <= 1; lane++) {
        if ((int)(hash32(h ^ (lane + 9) * 999u) % 100) < 30) {
            bool blocked = false;
            for (auto &cc : cars)
                if (cc.seg == seg && cc.lane == lane)
                    blocked = true;
            if (!blocked)
                coins.push_back({ seg, lane, false });
        }
    }
}

void resetGame() {

    mode = PLAYING;

    distanceScore = 0;
    coinScore = 0;
    currentSegment = 0;
    roadOffset = 0.0f;

    playerX = 0.0f;
    playerY = 0.5f;
    currentLane = 0;
    targetX = 0.0f;

    isJumping = false;
    velY = 0.0f;
    windmillAngle = 0.0f;

    cars.clear();
    coins.clear();

    for (long s = 5; s < visibleSegments + 60; s++)
        spawn(s);
}

/* ========================================================================
   DRAW WORLD (ALL SCENERY PRESERVED)
   ======================================================================== */

void drawWorld() {

    glPushMatrix();
    glTranslatef(0, 0, roadOffset);

    for (int i = -1; i < visibleSegments; i++) {

        long seg = currentSegment + i;

        float zn = -i * segmentLength;
        float zf = -(i + 1) * segmentLength;
        float zm = (zn + zf) * 0.5f;

        glColor3f(0.25f, 0.25f, 0.25f);
        glBegin(GL_QUADS);
        glVertex3f(-roadHalfWidth, 0, zn);
        glVertex3f( roadHalfWidth, 0, zn);
        glVertex3f( roadHalfWidth, 0, zf);
        glVertex3f(-roadHalfWidth, 0, zf);
        glEnd();

        if (i % 2 == 0) {
            glDisable(GL_LIGHTING);
            glColor3f(1, 1, 0);
            drawLineDDA(-1.0f, 0.02f, zn, -1.0f, 0.02f, zf);
            drawLineDDA( 1.0f, 0.02f, zn,  1.0f, 0.02f, zf);
            glEnable(GL_LIGHTING);
        }

        glColor3f(0.1f, 0.6f, 0.1f);
        glBegin(GL_QUADS);
        glVertex3f(-50.0f, -0.1f, zn);
        glVertex3f(-roadHalfWidth, -0.1f, zn);
        glVertex3f(-roadHalfWidth, -0.1f, zf);
        glVertex3f(-50.0f, -0.1f, zf);

        glVertex3f(roadHalfWidth, -0.1f, zn);
        glVertex3f(50.0f, -0.1f, zn);
        glVertex3f(50.0f, -0.1f, zf);
        glVertex3f(roadHalfWidth, -0.1f, zf);
        glEnd();

        uint32_t h = hash32((uint32_t)seg ^ 0x9e3779b9U);

        if ((h % 10) > 6)
            drawTree( roadHalfWidth + 3.0f + (h % 5), zm);

        if (((h >> 4) % 10) > 7)
            drawTree(-(roadHalfWidth + 3.0f + ((h >> 8) % 5)), zm);

        if (seg % 25 == 0)
            drawWindmill(roadHalfWidth + 9.0f, zm);

        if (seg % 17 == 0)
            drawCartoonHouse(-(roadHalfWidth + 12.0f), zm);

        if (seg % 23 == 0)
            drawCartoonHouse(roadHalfWidth + 12.0f, zm);

        if (seg % 11 == 0)
            drawCartoonCharacter(-(roadHalfWidth + 6.0f), zm);

        if (seg % 13 == 0)
            drawCartoonCharacter(roadHalfWidth + 6.0f, zm);
    }

    for (auto &c : cars) {
        float z = segmentZ(c.seg);
        if (z > -160 && z < 10)
            drawCar(laneX(c.lane), z);
    }

    for (auto &cn : coins) {
        if (!cn.collected) {
            float z = segmentZ(cn.seg);
            if (z > -160 && z < 10)
                drawCoin(laneX(cn.lane), z);
        }
    }

    glPopMatrix();
}

/* ========================================================================
   UPDATE LOOP (DIFFICULTY + COLLISION FIX + DAY CYCLE)
   ======================================================================== */

void update(int) {

    if (mode == COUNTDOWN) {
        countdownValue -= 0.016f;
        if (countdownValue <= 0)
            mode = PLAYING;
    }

    if (mode == PLAYING) {

        distanceScore++;
        windmillAngle += 2.0f;

        scrollSpeed =
            std::min(maxScrollSpeed,
                     baseScrollSpeed +
                     distanceScore * difficultyFactor);

        laneSpeed = 0.25f + scrollSpeed * 0.4f;

        roadOffset += scrollSpeed;

        if (roadOffset > segmentLength) {
            roadOffset -= segmentLength;
            currentSegment++;
            spawn(currentSegment + visibleSegments + 40);
        }

        dayCycle += 0.0005f;
        if (dayCycle > 6.283f)
            dayCycle = 0.0f;

        if (isJumping) {
            playerY += velY;
            velY -= GRAVITY;
            if (playerY <= 0.5f) {
                playerY = 0.5f;
                isJumping = false;
                velY = 0.0f;
            }
        }

        targetX = currentLane * laneWidth;

        if (playerX < targetX)
            playerX = std::min(targetX, playerX + laneSpeed);
        else if (playerX > targetX)
            playerX = std::max(targetX, playerX - laneSpeed);

        for (auto &c : cars) {
            if (c.lane == currentLane &&
                std::abs(segmentZ(c.seg) - (-roadOffset)) < 0.8f &&
                playerY <= 0.75f) {
                mode = GAMEOVER;
                saveHighScore();
            }
        }

        for (auto &cn : coins) {
            if (!cn.collected &&
                cn.lane == currentLane &&
                std::abs(segmentZ(cn.seg) - (-roadOffset)) < 0.8f) {
                cn.collected = true;
                coinScore++;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void display() {

    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(0.0, 4.0, 6.0,
              0.0, 0.0, -8.0,
              0.0, 1.0, 0.0);

    GLfloat lightPos[] = {30.0f, 60.0f, 30.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    drawAttractiveBackground();

    if (mode == MENU) {

        drawCenteredText("STREET RUNNER", h * 0.7f, 1,1,1);
        drawCenteredText("Press ENTER to Start Game", h * 0.55f, 1,1,1);
        drawCenteredText("Controls: A/D move, SPACE jump, ESC pause", h * 0.45f, 1,1,1);
        drawCenteredText("Developed by Nasif Abdullah", h * 0.35f, 1,1,1);

    }
    else {

        drawWorld();
        drawRobot();

        /* HUD */

        char s1[64], s2[64], s3[64];

        std::snprintf(s1, sizeof(s1), "Distance: %ld", distanceScore);
        std::snprintf(s2, sizeof(s2), "Coins: %ld", coinScore);
        std::snprintf(s3, sizeof(s3), "High Score: %ld", highScore);

        drawText(s1, 20, h - 50, 1,1,1);
        drawText(s2, 20, h - 80, 1,1,0);
        drawText(s3, 20, h - 110, 0,1,1);

        /* PAUSE SCREEN */

        if (mode == PAUSED) {
            drawCenteredText("PAUSED", h * 0.65f, 1,1,1);
            drawCenteredText("Press ESC to Resume", h * 0.55f, 1,1,0);
            drawCenteredText("Press Q to Quit", h * 0.45f, 1,0,0);
        }

        /* COUNTDOWN SCREEN */

        if (mode == COUNTDOWN) {
            char cStr[8];
            std::snprintf(cStr, sizeof(cStr), "%d",
                          (int)ceil(countdownValue));

            drawCenteredText("GET READY", h * 0.65f, 1,1,1);
            drawCenteredText(cStr, h * 0.55f, 1,1,0);
        }

        /* GAME OVER SCREEN */

        if (mode == GAMEOVER) {
            drawCenteredText("GAME OVER", h * 0.6f, 1,0,0);
            drawCenteredText("Press R to Restart", h * 0.5f, 1,1,1);
        }
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0,
                   (double)w / (double)h,
                   1.0,
                   300.0);

    glMatrixMode(GL_MODELVIEW);
}
void keys(unsigned char k, int, int) {

    if (k == 27) {
        if (mode == PLAYING)
            mode = PAUSED;
        else if (mode == PAUSED) {
            mode = COUNTDOWN;
            countdownValue = 3.0f;
        }
        return;
    }

    if (mode == MENU && k == 13)
        resetGame();

    if (mode == GAMEOVER && (k == 'r' || k == 'R'))
        resetGame();

    if (mode == PLAYING) {

        if ((k == 'a' || k == 'A') && currentLane > -1)
            currentLane--;

        if ((k == 'd' || k == 'D') && currentLane < 1)
            currentLane++;

        if (k == ' ' && !isJumping) {
            isJumping = true;
            velY = JUMP_FORCE;
        }
    }
}

/* ========================================================================
   MAIN
   ======================================================================== */

int main(int argc, char** argv) {

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1920, 1080);
    glutCreateWindow("Street Runner - Full Advanced");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat amb[] = {0.4f,0.4f,0.4f,1};
    GLfloat dif[] = {0.8f,0.8f,0.8f,1};

    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    loadHighScore();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keys);
    glutTimerFunc(0, update, 0);

    glutMainLoop();
    return 0;
}
