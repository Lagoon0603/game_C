#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>



// 画面・システム設定
#define INITIAL_SCREEN_WIDTH 800
#define INITIAL_SCREEN_HEIGHT 450
#define MAX_BULLETS 300
#define MAX_ENEMIES 100
#define MAX_PARTICLES 600
#define MAX_ITEMS 100

// バランス調整
#define KILLS_TO_BOSS_BASE 10
#define TRAIL_LENGTH 10
#define FIELD_LIMIT 45.0f

// カラー設定
#define COL_NEON_CYAN   (Color){ 0, 255, 255, 255 }
#define COL_NEON_PINK   (Color){ 255, 0, 255, 255 }
#define COL_NEON_GREEN  (Color){ 0, 255, 100, 255 }
#define COL_NEON_PURPLE (Color){ 150, 0, 255, 255 }
#define COL_NEON_ORANGE (Color){ 255, 100, 0, 255 }
#define COL_DARK_BG     (Color){ 5, 5, 10, 255 }



// 構造体定義
// ゲーム遷移
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PVP,
    STATE_BOSS_INTRO,
    STATE_STAGE_CLEAR,
    STATE_GAMEOVER,
    STATE_PVP_RESULT,
    STATE_PAUSED
} GameState;

// 難易度
typedef enum {
    MODE_NORMAL,
    MODE_HARD
} DifficultyMode;

// プレイヤー情報
typedef struct {
    Vector3 position;
    float speed;
    int hp;
    int max_hp;
    int level;
    int exp;
    int next_level_exp;
    int damage;
    int weapon_type; 
    float shoot_cooldown;
    float dash_cooldown;
    float dash_duration;
    Vector3 dash_dir;
    float walk_anim_timer;
    float facing_angle;
    float invincible_timer;
    Vector3 trail_pos[TRAIL_LENGTH];
    int trail_idx;
} Player;

typedef enum { ENEMY_DRONE, ENEMY_TANK, ENEMY_BOSS } EnemyType;

// 敵情報
typedef struct {
    Vector3 position;
    bool active;
    EnemyType type;
    float speed;
    int hp;
    int max_hp;
    Vector3 knockback;
    float flash_timer;
    float anim_timer;
    float vertical_speed; 
    bool is_grounded;     
    float shoot_cooldown;
    float attack_range;
} Enemy;

// 弾設定
typedef struct {
    Vector3 position;
    Vector3 velocity;
    bool active;
    float life_time;
    bool is_enemy_bullet;
    bool is_p2_bullet;
} Bullet;

// エフェクト
typedef struct {
    Vector3 position;
    Vector3 velocity;
    Color color;
    bool active;
    float life;
    float max_life; 
    float size;
} Particle;

typedef enum { ITEM_HEAL, ITEM_EXP } ItemType;

// アイテム
typedef struct {
    Vector3 position;
    bool active;
    ItemType type;
    float life_time;
    float angle;
} Item;



// グローバル変数
GameState current_state = STATE_TITLE;
GameState previous_state = STATE_TITLE;
DifficultyMode difficulty = MODE_NORMAL;

// エンティティ
Player player = { 0 };
Player player2 = { 0 };
Enemy enemies[MAX_ENEMIES] = { 0 };
Bullet bullets[MAX_BULLETS] = { 0 };
Particle particles[MAX_PARTICLES] = { 0 };
Item items[MAX_ITEMS] = { 0 };

// カメラ
Camera3D camera = { 0 };
Camera3D camera2 = { 0 };

// 進行状況
float game_time = 0.0f;
int winner_id = 0;
int current_stage = 1;
int stage_kills = 0;
int kills_required_for_boss = KILLS_TO_BOSS_BASE;
bool boss_spawned = false;
float state_timer = 0.0f;
float enemy_spawn_timer = 0.0f;
float screen_shake = 0.0f;
float camera_angle_rad = 0.0f;

void InitGame(bool reset_player);
void UpdateGame();
void UpdateGamePvP();
void UpdatePaused();
void DrawGame();
void DrawGamePvP();
void DrawPaused();
void DrawScene(Camera3D cam, bool draw_cursor);
void UpdateTitle();
void DrawTitle();
void DrawMecha(Vector3 pos, float angle, Color color, float anim_time, EnemyType type);
void SpawnEnemy(bool force_boss);
void SpawnBullet(Vector3 pos, Vector3 direction, bool is_enemy, bool is_p2);
void SpawnExplosion(Vector3 pos, Color color, int count);
void SpawnItem(Vector3 pos);
void ResetStage();
void AddScreenShake(float amount);
void UpdateTrail(Player *p);
void DrawCyberGrid(Vector3 centerPos);



// メイン
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT, "Voxel Survivor 6.1 - Bug Fixes");
    HideCursor();
    SetTargetFPS(60); 
    
    int monitor = GetCurrentMonitor();
    int x = (GetMonitorWidth(monitor) - INITIAL_SCREEN_WIDTH) / 2;
    int y = (GetMonitorHeight(monitor) - INITIAL_SCREEN_HEIGHT) / 2;
    SetWindowPosition(x, y);

    camera.position = (Vector3){ 0.0f, 20.0f, 20.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        if (screen_shake > 0) screen_shake -= GetFrameTime() * 30.0f;
        if (screen_shake < 0) screen_shake = 0;

        if (IsKeyPressed(KEY_TAB)) {
            if (current_state == STATE_PAUSED) {
                current_state = previous_state;
            } 
            else if (current_state == STATE_PLAYING || current_state == STATE_PVP || 
                     current_state == STATE_BOSS_INTRO || current_state == STATE_STAGE_CLEAR) {
                previous_state = current_state;
                current_state = STATE_PAUSED;
            }
        }

        switch (current_state) {
            case STATE_TITLE: UpdateTitle(); BeginDrawing(); DrawTitle(); EndDrawing(); break;
            case STATE_PVP: UpdateGamePvP(); BeginDrawing(); DrawGamePvP(); EndDrawing(); break;
            case STATE_PAUSED: UpdatePaused(); BeginDrawing(); if (previous_state == STATE_PVP) DrawGamePvP(); else DrawGame(); DrawPaused(); EndDrawing(); break;
            case STATE_PVP_RESULT: UpdateGamePvP(); BeginDrawing(); DrawGamePvP(); EndDrawing(); break;
            default: UpdateGame(); BeginDrawing(); DrawGame(); EndDrawing(); break;
        }
    }
    CloseWindow();
    return 0;
}

void UpdatePaused() { 
    if (IsKeyPressed(KEY_R)) {
        current_state = STATE_TITLE;
        camera_angle_rad = 0.0f;
    }
}

void DrawPaused() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 150});
    DrawRectangleLines(w/2 - 150, h/2 - 60, 300, 150, COL_NEON_CYAN);
    DrawText("PAUSED", w/2 - MeasureText("PAUSED", 40)/2, h/2 - 40, 40, COL_NEON_CYAN);
    DrawText("TAB: RESUME", w/2 - MeasureText("TAB: RESUME", 20)/2, h/2 + 10, 20, WHITE);
    DrawText("R: TITLE", w/2 - MeasureText("R: TITLE", 20)/2, h/2 + 40, 20, WHITE);
}

void UpdateTitle() {
    float time = GetTime();
    camera.position.x = sinf(time * 0.3f) * 35.0f;
    camera.position.z = cosf(time * 0.3f) * 35.0f;
    camera.target = (Vector3){ 0, 0, 0 };

    if (IsKeyDown(KEY_N)) { difficulty = MODE_NORMAL; InitGame(true); current_state = STATE_PLAYING; }
    if (IsKeyDown(KEY_H)) { difficulty = MODE_HARD; InitGame(true); current_state = STATE_PLAYING; }
    if (IsKeyDown(KEY_P)) {
        InitGame(true);
        player.position = (Vector3){ -10, 0, 0 };
        player2.position = (Vector3){ 10, 0, 0 };
        player2.hp = 100; player2.max_hp = 100; player2.speed = 10.0f;
        camera2 = camera;
        current_state = STATE_PVP;
    }
}

void DrawTitle() {
    int w = GetScreenWidth();
    ClearBackground(COL_DARK_BG);
    
    BeginMode3D(camera);
        DrawCyberGrid((Vector3){0,0,GetTime()*5.0f}); 
        DrawMecha((Vector3){5,0,0}, 0, COL_NEON_PINK, GetTime(), ENEMY_DRONE);
        DrawMecha((Vector3){-5,0,0}, 3.14, COL_NEON_PURPLE, GetTime(), ENEMY_TANK);
        DrawMecha((Vector3){0,5,-10}, 0, COL_NEON_ORANGE, GetTime(), ENEMY_BOSS);
    EndMode3D();

    DrawText("VOXEL SURVIVOR", w/2 - MeasureText("VOXEL SURVIVOR", 60)/2, 100, 60, COL_NEON_CYAN);
    DrawText("NEON EDITION", w/2 - MeasureText("NEON EDITION", 40)/2, 170, 40, COL_NEON_PINK);
    DrawText("[N] NORMAL", w/2 - 150, 300, 30, WHITE);
    DrawText("[H] HARD", w/2 - 150, 350, 30, COL_NEON_ORANGE);
    DrawText("[P] VS 2P", w/2 - 150, 400, 30, COL_NEON_GREEN);
}

void InitGame(bool reset_player) {
    if (reset_player) {
        player.position = (Vector3){ 0, 0, 0 };
        player.speed = 12.0f;
        player.hp = 100; player.max_hp = 100;
        player.level = 1; player.exp = 0; 
        player.next_level_exp = 5;
        player.damage = 20;
        player.weapon_type = 0;
        player.shoot_cooldown = 0.0f;
        player.dash_cooldown = 0; player.dash_duration = 0;
        player.invincible_timer = 0;
        for(int i=0; i<TRAIL_LENGTH; i++) player.trail_pos[i] = player.position;
        current_stage = 1;
        player2 = player; 
    }
    ResetStage();
    camera.position = (Vector3){ 0.0f, 25.0f, 18.0f }; 
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 50.0f;
    game_time = 0.0f;
    screen_shake = 0.0f;
}

void ResetStage() {
    stage_kills = 0;
    boss_spawned = false;
    kills_required_for_boss = KILLS_TO_BOSS_BASE + (current_stage - 1) * 5; 
    enemy_spawn_timer = 0.0f;
    for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = false;
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
    for(int i=0; i<MAX_PARTICLES; i++) particles[i].active = false;
    for(int i=0; i<MAX_ITEMS; i++) items[i].active = false;
}

void AddScreenShake(float amount) {
    screen_shake = amount;
    if(screen_shake > 2.0f) screen_shake = 2.0f;
}

void UpdateTrail(Player *p) {
    if (p->dash_duration > 0 || (int)(game_time * 10) % 2 == 0) { 
        p->trail_idx = (p->trail_idx + 1) % TRAIL_LENGTH;
        p->trail_pos[p->trail_idx] = p->position;
    }
}

void DrawCyberGrid(Vector3 centerPos) {
    int slices = 20;
    float spacing = 4.0f;
    float offsetX = centerPos.x - fmodf(centerPos.x, spacing);
    float offsetZ = centerPos.z - fmodf(centerPos.z, spacing);

    rlBegin(RL_LINES);
    for (int i = -slices; i <= slices; i++) {
        float xPos = offsetX + i * spacing;
        
        float dist = fabsf(xPos - centerPos.x);
        float alpha = 1.0f - (dist / (slices * spacing));
        if(alpha < 0) alpha = 0;
        
        Color c = ColorAlpha(COL_NEON_PURPLE, alpha * 0.5f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(xPos, 0, centerPos.z - slices * spacing);
        rlVertex3f(xPos, 0, centerPos.z + slices * spacing);
    }

    for (int i = -slices; i <= slices; i++) {
        float zPos = offsetZ + i * spacing;
        
        float dist = fabsf(zPos - centerPos.z);
        float alpha = 1.0f - (dist / (slices * spacing)); 
        if(alpha < 0) alpha = 0;
        
        Color c = ColorAlpha(COL_NEON_PURPLE, alpha * 0.5f);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(centerPos.x - slices * spacing, 0, zPos);
        rlVertex3f(centerPos.x + slices * spacing, 0, zPos);
    }
    rlEnd();
}

void DrawMecha(Vector3 pos, float angle, Color color, float anim_time, EnemyType type) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(angle * RAD2DEG, 0, 1, 0);
    
    float scale = (type == ENEMY_BOSS) ? 2.5f : 1.0f;
    rlScalef(scale, scale, scale);

    float bounce = sinf(anim_time * 15.0f) * 0.1f;
    if (type == ENEMY_TANK) bounce *= 0.2f;
    rlTranslatef(0, bounce, 0);

    float bodySize = (type == ENEMY_TANK || type == ENEMY_BOSS) ? 1.5f : 0.8f;
    
    DrawCube((Vector3){0, bodySize, 0}, bodySize, bodySize, bodySize, color);
    DrawCubeWires((Vector3){0, bodySize, 0}, bodySize, bodySize, bodySize, WHITE); 

    Vector3 headPos = {0, bodySize * 1.8f, 0};
    float headSize = bodySize * 0.6f;
    DrawCube(headPos, headSize, headSize, headSize, GRAY);
    DrawCube((Vector3){0, headPos.y, headSize/2 + 0.05f}, headSize*0.8f, headSize*0.3f, 0.1f, COL_NEON_CYAN);

    float legOffset = bodySize * 0.4f;
    float legLength = (type == ENEMY_TANK) ? 0.8f : 1.0f;
    float legAngle = sinf(anim_time * 15.0f) * 30.0f;

    rlPushMatrix();
    rlTranslatef(-legOffset, bodySize/2, 0);
    rlRotatef(legAngle, 1, 0, 0);
    DrawCube((Vector3){0, -legLength/2, 0}, 0.3f, legLength, 0.3f, DARKGRAY);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(legOffset, bodySize/2, 0);
    rlRotatef(-legAngle, 1, 0, 0);
    DrawCube((Vector3){0, -legLength/2, 0}, 0.3f, legLength, 0.3f, DARKGRAY);
    rlPopMatrix();
    
    if (type == ENEMY_BOSS) {
        rlPushMatrix();
        rlTranslatef(0, bodySize * 1.5f, -0.5f);
        DrawCube((Vector3){0,0,0}, 1.2f, 1.2f, 0.5f, DARKGRAY);
        DrawCube((Vector3){0.8f, 0.5f, 0.2f}, 0.2f, 0.2f, 1.0f, COL_NEON_ORANGE);
        DrawCube((Vector3){-0.8f, 0.5f, 0.2f}, 0.2f, 0.2f, 1.0f, COL_NEON_ORANGE);
        rlPopMatrix();
    }
    rlPopMatrix();
    
    rlBegin(RL_TRIANGLES);
        Color shadow = (Color){0,0,0, 100};
        float shadowSize = bodySize * 0.8f;
        rlColor4ub(shadow.r, shadow.g, shadow.b, shadow.a);
        rlVertex3f(pos.x - shadowSize, 0.05f, pos.z - shadowSize);
        rlVertex3f(pos.x - shadowSize, 0.05f, pos.z + shadowSize);
        rlVertex3f(pos.x + shadowSize, 0.05f, pos.z + shadowSize);
        rlVertex3f(pos.x - shadowSize, 0.05f, pos.z - shadowSize);
        rlVertex3f(pos.x + shadowSize, 0.05f, pos.z + shadowSize);
        rlVertex3f(pos.x + shadowSize, 0.05f, pos.z - shadowSize);
    rlEnd();
}

void UpdateGame() {
    float dt = GetFrameTime();
    game_time += dt;

    // 特殊状態
    if (current_state == STATE_GAMEOVER) {
        if (IsKeyPressed(KEY_R)) {
            current_state = STATE_TITLE;
            camera_angle_rad = 0.0f;
        }
        return;
    }
    if (current_state == STATE_STAGE_CLEAR) {
        state_timer += dt;
        if (state_timer > 3.0f) {
            current_stage++;
            ResetStage();
            current_state = STATE_PLAYING;
        }
        return;
    }
    if (current_state == STATE_BOSS_INTRO) {
        AddScreenShake(0.1f); 
        state_timer += dt;
        if (state_timer > 2.0f) {
            SpawnEnemy(true);
            current_state = STATE_PLAYING;
        }
        return; 
    }

    // 視点移動
    if (IsKeyDown(KEY_E)) camera_angle_rad -= 2.0f * dt;
    if (IsKeyDown(KEY_Q)) camera_angle_rad += 2.0f * dt;

    float camDistH = 18.0f;
    float camHeight = 25.0f;
    float camOffsetX = sinf(camera_angle_rad) * camDistH;
    float camOffsetZ = cosf(camera_angle_rad) * camDistH;

    Vector3 targetCamPos = {
        player.position.x + camOffsetX,
        camHeight,
        player.position.z + camOffsetZ
    };

    Ray ray = GetMouseRay(GetMousePosition(), camera);
    float t = -ray.position.y / ray.direction.y;
    Vector3 aim_point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    
    float shakeX = (float)GetRandomValue(-10, 10) * 0.05f * screen_shake;
    float shakeZ = (float)GetRandomValue(-10, 10) * 0.05f * screen_shake;
    Vector3 finalCamPos = Vector3Add(targetCamPos, (Vector3){shakeX, 0, shakeZ});

    camera.position = Vector3Lerp(camera.position, finalCamPos, 0.1f);
    camera.target = Vector3Lerp(camera.target, player.position, 0.1f);

    Vector3 diff = Vector3Subtract(aim_point, player.position);
    player.facing_angle = -atan2f(diff.z, diff.x) + PI/2;

    UpdateTrail(&player);
    if (player.invincible_timer > 0) player.invincible_timer -= dt;
    
    //プレイヤー移動
    Vector3 move = {0};
    Vector3 forward = { -sinf(camera_angle_rad), 0, -cosf(camera_angle_rad) };
    Vector3 right   = { cosf(camera_angle_rad),  0, -sinf(camera_angle_rad) };

    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);

    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT)) && player.dash_cooldown <= 0) {
        player.dash_duration = 0.2f;
        player.dash_cooldown = 1.5f;
        
        if (Vector3Length(move) > 0) {
            player.dash_dir = Vector3Normalize(move);
        } else {
            if (Vector3Length(diff) > 0.1f) player.dash_dir = Vector3Normalize(diff);
            else player.dash_dir = (Vector3){0, 0, 1};
        }
        SpawnExplosion(player.position, WHITE, 5);
        AddScreenShake(0.2f);
    }
    
    if (player.dash_duration > 0) {
        player.dash_duration -= dt;
        player.position = Vector3Add(player.position, Vector3Scale(player.dash_dir, player.speed * 3.0f * dt));
    } else {
        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
            player.position = Vector3Add(player.position, Vector3Scale(move, player.speed * dt));
            player.walk_anim_timer += dt;
        } else player.walk_anim_timer = 0;
    }

    // 攻撃
    if (player.shoot_cooldown > 0) player.shoot_cooldown -= dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.shoot_cooldown <= 0) {
        Vector3 aim_dir = Vector3Normalize(Vector3Subtract(aim_point, player.position));
        aim_dir.y = 0;
        SpawnBullet(player.position, aim_dir, false, false);
        player.shoot_cooldown = 0.15f; 
        if (player.level > 5) player.shoot_cooldown = 0.12f;
        if (player.level > 10) player.shoot_cooldown = 0.08f;
        AddScreenShake(0.1f);
    }

    // ヒット判定
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector3Add(bullets[i].position, Vector3Scale(bullets[i].velocity, dt));
        bullets[i].life_time -= dt;

        if (bullets[i].life_time <= 0) { 
            bullets[i].active = false;
            continue;
        }
        if (bullets[i].is_enemy_bullet && player.invincible_timer <= 0 && player.dash_duration <= 0) {
            Vector3 playerCenter = { player.position.x, 1.0f, player.position.z };
            if (Vector3Distance(bullets[i].position, playerCenter) < 2.0f) { 
                player.hp -= 10;
                player.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player.position, COL_NEON_PINK, 15);
                AddScreenShake(0.8f);
                if (player.hp <= 0) current_state = STATE_GAMEOVER;
            }
        }
    }

    // アイテム取得
    for (int i=0; i<MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        items[i].angle += dt * 90.0f;
        items[i].life_time -= dt;
        if (items[i].life_time <= 0) items[i].active = false;
        if (Vector3Distance(player.position, items[i].position) < 3.0f) {
            if (items[i].type == ITEM_HEAL) {
                player.hp += 30;
                if(player.hp > player.max_hp) player.hp = player.max_hp;
                SpawnExplosion(player.position, COL_NEON_GREEN, 10);
            } 
            else if (items[i].type == ITEM_EXP) {
                player.exp += 1;
                
                if (player.exp >= player.next_level_exp) {
                    player.level++;
                    player.exp = 0;
                    player.next_level_exp += 5; 
                    player.max_hp += 10;
                    player.hp = player.max_hp;
                    player.damage += 5;
                    
                    SpawnExplosion(player.position, GOLD, 20);
                }
                SpawnExplosion(player.position, COL_NEON_CYAN, 5);
            }
            items[i].active = false;
        }
    }

    // 敵のスポーン
    if (!boss_spawned) {
        if (stage_kills >= kills_required_for_boss) {
            current_state = STATE_BOSS_INTRO;
            state_timer = 0.0f;
            for(int i=0; i<MAX_ENEMIES; i++) if(enemies[i].active) {
                enemies[i].hp = 0;
                SpawnExplosion(enemies[i].position, COL_NEON_ORANGE, 5);
                enemies[i].active = false;
            }
        } else {
            enemy_spawn_timer += dt;
            float spawnInterval = 0.5f - (current_stage * 0.05f);
            if (spawnInterval < 0.1f) spawnInterval = 0.1f;
            if (difficulty == MODE_HARD) spawnInterval *= 0.7f;

            if (enemy_spawn_timer > spawnInterval) {
                SpawnEnemy(false);
                enemy_spawn_timer = 0.0f;
            }
        }
    }

    // 敵の制御
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (!enemies[i].is_grounded) {
            enemies[i].vertical_speed -= 40.0f * dt;
            enemies[i].position.y += enemies[i].vertical_speed * dt;
            if (enemies[i].position.y <= 0) {
                enemies[i].position.y = 0;
                enemies[i].is_grounded = true;
                SpawnExplosion(enemies[i].position, LIGHTGRAY, 5);
            } else continue;
        }
        Vector3 to_player = Vector3Subtract(player.position, enemies[i].position);
        float dist = Vector3Length(to_player);
        to_player = Vector3Normalize(to_player);
        
        enemies[i].anim_timer += dt;
        if (dist > 1.5f) {
            Vector3 move = Vector3Scale(to_player, enemies[i].speed * dt);
            Vector3 knock = Vector3Scale(enemies[i].knockback, dt);
            enemies[i].position = Vector3Add(enemies[i].position, Vector3Add(move, knock));
        }
        enemies[i].knockback = Vector3Scale(enemies[i].knockback, 0.85f);
        if (enemies[i].flash_timer > 0) enemies[i].flash_timer -= dt;

        if (enemies[i].shoot_cooldown > 0) enemies[i].shoot_cooldown -= dt;
        if (enemies[i].type == ENEMY_BOSS || (enemies[i].type == ENEMY_TANK && difficulty == MODE_HARD)) {
            if (enemies[i].shoot_cooldown <= 0 && dist < enemies[i].attack_range) {
                SpawnBullet(enemies[i].position, to_player, true, false);
                enemies[i].shoot_cooldown = (enemies[i].type == ENEMY_BOSS) ? 1.0f : 2.5f;
            }
        }
        if (dist < 1.5f && player.dash_duration <= 0 && player.invincible_timer <= 0) {
            player.hp -= 5;
            player.invincible_timer = 0.5f;
            AddScreenShake(0.5f);
            if (player.hp <= 0) current_state = STATE_GAMEOVER;
        }

        // プレイヤーと敵の当たり判定
        float hitSize = (enemies[i].type == ENEMY_BOSS) ? 2.5f : 1.0f;
        BoundingBox box = {
            (Vector3){enemies[i].position.x - hitSize, 0, enemies[i].position.z - hitSize},
            (Vector3){enemies[i].position.x + hitSize, hitSize * 2.5f, enemies[i].position.z + hitSize}
        };
        for (int b=0; b<MAX_BULLETS; b++) {
            if (!bullets[b].active || bullets[b].is_enemy_bullet) continue;
            if (CheckCollisionBoxSphere(box, bullets[b].position, 0.5f)) {
                bullets[b].active = false;
                enemies[i].hp -= player.damage; 
                enemies[i].flash_timer = 0.1f;
                if (enemies[i].type != ENEMY_BOSS) {
                    Vector3 push = Vector3Normalize(bullets[b].velocity);
                    enemies[i].knockback = Vector3Add(enemies[i].knockback, Vector3Scale(push, 15.0f));
                }
                SpawnExplosion(bullets[b].position, COL_NEON_CYAN, 3);
                
                if (enemies[i].hp <= 0) {
                    enemies[i].active = false;
                    SpawnExplosion(enemies[i].position, enemies[i].type == ENEMY_TANK ? COL_NEON_PURPLE : COL_NEON_ORANGE, 20);
                    AddScreenShake(0.3f);
                    if (enemies[i].type == ENEMY_BOSS) {
                        boss_spawned = false; 
                        current_state = STATE_STAGE_CLEAR;
                        state_timer = 0;
                        AddScreenShake(2.0f);
                    } else {
                        stage_kills++;
                        if (GetRandomValue(0, 100) < 50) SpawnItem(enemies[i].position);
                    }
                }
            }
        }
    }
    for(int i=0; i<MAX_PARTICLES; i++){
        if(!particles[i].active) continue;
        particles[i].position = Vector3Add(particles[i].position, Vector3Scale(particles[i].velocity, dt));
        particles[i].life -= dt;
        if(particles[i].life <= 0) particles[i].active = false;
    }
}

// ２人対戦
void UpdateGamePvP() {
    float dt = GetFrameTime();
    if (current_state == STATE_PVP_RESULT) {
        if (IsKeyPressed(KEY_R)) {
            current_state = STATE_TITLE;
            camera_angle_rad = 0.0f;
        }
        return;
    }
    
    // P1
    UpdateTrail(&player);
    if (player.invincible_timer > 0) player.invincible_timer -= dt;
    if (IsKeyPressed(KEY_SPACE) && player.dash_cooldown <= 0) {
        player.dash_duration = 0.2f; player.dash_cooldown = 1.5f;
        Vector3 input = {0};
        if (IsKeyDown(KEY_W)) input.z -= 1; if (IsKeyDown(KEY_S)) input.z += 1;
        if (IsKeyDown(KEY_A)) input.x -= 1; if (IsKeyDown(KEY_D)) input.x += 1;
        if (Vector3Length(input) > 0) player.dash_dir = Vector3Normalize(input);
        else player.dash_dir = (Vector3){0,0,-1};
        AddScreenShake(0.2f);
    }
    if (player.dash_duration > 0) {
        player.dash_duration -= dt;
        player.position = Vector3Add(player.position, Vector3Scale(player.dash_dir, player.speed * 3.0f * dt));
    } else {
        Vector3 move = {0};
        if (IsKeyDown(KEY_W)) move.z -= 1; if (IsKeyDown(KEY_S)) move.z += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1; if (IsKeyDown(KEY_D)) move.x += 1;
        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
            player.position = Vector3Add(player.position, Vector3Scale(move, player.speed * dt));
            player.walk_anim_timer += dt;
        } else player.walk_anim_timer = 0;
    }
    if (player.dash_cooldown > 0) player.dash_cooldown -= dt;

    Vector2 mousePos = GetMousePosition();
    float screenW = (float)GetScreenWidth();
    if (mousePos.x > screenW / 2.0f) mousePos.x = screenW / 2.0f;
    mousePos.x *= 2.0f;
    Ray ray = GetMouseRay(mousePos, camera);
    float t = -ray.position.y / ray.direction.y;
    Vector3 aim_point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    Vector3 d = Vector3Subtract(aim_point, player.position);
    d.y = 0; 
    player.facing_angle = -atan2f(d.z, d.x) + PI/2;
    if (player.shoot_cooldown > 0) player.shoot_cooldown -= dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.shoot_cooldown <= 0) {
        SpawnBullet(player.position, Vector3Normalize(d), false, false);
        player.shoot_cooldown = 0.3f;
    }

    // P2
    UpdateTrail(&player2);
    if (player2.invincible_timer > 0) player2.invincible_timer -= dt;
    if (IsKeyPressed(KEY_ENTER) && player2.dash_cooldown <= 0) {
        player2.dash_duration = 0.2f; player2.dash_cooldown = 1.5f;
        Vector3 input = {0};
        if (IsKeyDown(KEY_UP)) input.z -= 1; if (IsKeyDown(KEY_DOWN)) input.z += 1;
        if (IsKeyDown(KEY_LEFT)) input.x -= 1; if (IsKeyDown(KEY_RIGHT)) input.x += 1;
        if (Vector3Length(input) > 0) player2.dash_dir = Vector3Normalize(input);
        else player2.dash_dir = (Vector3){0,0,1};
        AddScreenShake(0.2f);
    }
    if (player2.dash_duration > 0) {
        player2.dash_duration -= dt;
        player2.position = Vector3Add(player2.position, Vector3Scale(player2.dash_dir, player2.speed * 3.0f * dt));
    } else {
        Vector3 move = {0};
        if (IsKeyDown(KEY_UP)) move.z -= 1; if (IsKeyDown(KEY_DOWN)) move.z += 1;
        if (IsKeyDown(KEY_LEFT)) move.x -= 1; if (IsKeyDown(KEY_RIGHT)) move.x += 1;
        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
            player2.position = Vector3Add(player2.position, Vector3Scale(move, player2.speed * dt));
            player2.walk_anim_timer += dt;
        } else player2.walk_anim_timer = 0;
    }
    if (player2.dash_cooldown > 0) player2.dash_cooldown -= dt;
    Vector3 toP1 = Vector3Subtract(player.position, player2.position);
    player2.facing_angle = -atan2f(toP1.z, toP1.x) + PI/2;
    if (player2.shoot_cooldown > 0) player2.shoot_cooldown -= dt;
    if (IsKeyDown(KEY_RIGHT_SHIFT) && player2.shoot_cooldown <= 0) {
        SpawnBullet(player2.position, Vector3Normalize(toP1), false, true);
        player2.shoot_cooldown = 0.3f;
    }

    Vector3 p1CamBase = Vector3Add(player.position, (Vector3){0, 20, 15});
    Vector3 p2CamBase = Vector3Add(player2.position, (Vector3){0, 20, 15});
    float shakeX = (float)GetRandomValue(-10, 10) * 0.05f * screen_shake;
    float shakeZ = (float)GetRandomValue(-10, 10) * 0.05f * screen_shake;
    camera.target = player.position;
    camera.position = Vector3Add(p1CamBase, (Vector3){shakeX, 0, shakeZ});
    camera2.target = player2.position;
    camera2.position = Vector3Add(p2CamBase, (Vector3){shakeX, 0, shakeZ});

    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector3Add(bullets[i].position, Vector3Scale(bullets[i].velocity, dt));
        bullets[i].life_time -= dt;
        if (bullets[i].life_time <= 0) { bullets[i].active = false; continue; }

        Vector3 p1Center = {player.position.x, 1, player.position.z};
        Vector3 p2Center = {player2.position.x, 1, player2.position.z};

        if (current_state == STATE_PVP_RESULT) continue;

        if (bullets[i].is_p2_bullet && player.invincible_timer <= 0 && player.dash_duration <= 0) {
            if (Vector3Distance(bullets[i].position, p1Center) < 2.0f) {
                player.hp -= 5; player.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player.position, COL_NEON_PINK, 10);
                AddScreenShake(0.5f);
                if (player.hp <= 0) { current_state = STATE_PVP_RESULT; winner_id = 2; }
            }
        }
        else if (!bullets[i].is_p2_bullet && player2.invincible_timer <= 0 && player2.dash_duration <= 0) {
            if (Vector3Distance(bullets[i].position, p2Center) < 2.0f) {
                player2.hp -= 5; player2.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player2.position, COL_NEON_PINK, 10);
                AddScreenShake(0.5f);
                if (player2.hp <= 0) { current_state = STATE_PVP_RESULT; winner_id = 1; }
            }
        }
    }
    for(int i=0; i<MAX_PARTICLES; i++){
        if(!particles[i].active) continue;
        particles[i].position = Vector3Add(particles[i].position, Vector3Scale(particles[i].velocity, dt));
        particles[i].life -= dt;
        if(particles[i].life <= 0) particles[i].active = false;
    }
}

// UI
void DrawGame() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    ClearBackground(COL_DARK_BG);

    BeginMode3D(camera);
    DrawScene(camera, true);
    EndMode3D();

    DrawText(TextFormat("STAGE %d", current_stage), 20, 20, 30, WHITE);
    DrawText(TextFormat("HP: %d/%d", player.hp, player.max_hp), 20, 60, 40, (player.hp < 30 ? COL_NEON_PINK : COL_NEON_GREEN));
    DrawText(TextFormat("LV. %d", player.level), 20, 110, 40, GOLD);

    float expRatio = (float)player.exp / (float)player.next_level_exp;
    if (expRatio > 1.0f) expRatio = 1.0f;

    DrawRectangle(140, 120, 200, 20, (Color){ 50, 40, 0, 200 });
    DrawRectangle(140, 120, (int)(200 * expRatio), 20, GOLD);
    DrawRectangleLines(140, 120, 200, 20, WHITE);
    DrawText(TextFormat("EXP: %d/%d", player.exp, player.next_level_exp), 150, 122, 10, BLACK);

    if (!boss_spawned) {
        float progress = (float)stage_kills / kills_required_for_boss;
        if(progress > 1.0) progress = 1.0;
        DrawRectangle(w/2 - 150, 50, 300, 20, DARKGRAY);
        DrawRectangle(w/2 - 150, 50, 300 * progress, 20, COL_NEON_ORANGE);
        DrawRectangleLines(w/2 - 150, 50, 300, 20, WHITE);
    } else DrawText("!! WARNING: BOSS ACTIVE !!", w/2 - 200, 30, 30, COL_NEON_PINK);

    if (current_state == STATE_BOSS_INTRO) {
        DrawRectangle(0, h/2 - 60, w, 120, (Color){0,0,0,150});
        DrawText("WARNING", w/2 - MeasureText("WARNING", 50)/2, h/2 - 40, 50, COL_NEON_PINK);
    }
    if (current_state == STATE_STAGE_CLEAR) {
        DrawRectangle(0, h/2 - 60, w, 120, (Color){255,255,255,150});
        DrawText("STAGE CLEAR!", w/2 - MeasureText("STAGE CLEAR!", 50)/2, h/2 - 20, 50, GOLD);
    }
    if (current_state == STATE_GAMEOVER) {
        DrawRectangle(0, 0, w, h, (Color){0,0,0,200});
        DrawText("GAME OVER", w/2 - MeasureText("GAME OVER", 80)/2, h/2 - 50, 80, COL_NEON_PINK);
        DrawText("PRESS 'R' TO RETURN TITLE", w/2 - MeasureText("PRESS 'R' TO RETURN TITLE", 20)/2, h/2 + 50, 20, GRAY);
    }
}

void DrawGamePvP() {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int renderW = GetRenderWidth();
    int renderH = GetRenderHeight();
    
    ClearBackground(COL_DARK_BG);
    
    // P1
    rlViewport(0, 0, renderW/2, renderH); 
    BeginScissorMode(0, 0, screenW/2, screenH);
        ClearBackground(COL_DARK_BG);
        BeginMode3D(camera);
            DrawScene(camera, true);
        EndMode3D();
        DrawRectangleLines(0, 0, screenW/2, screenH, COL_NEON_CYAN); 
    EndScissorMode();

    // P2
    rlViewport(renderW/2, 0, renderW/2, renderH);
    BeginScissorMode(screenW/2, 0, screenW/2, screenH);
        ClearBackground(COL_DARK_BG);
        BeginMode3D(camera2);
            DrawScene(camera2, false);
        EndMode3D();
        DrawRectangleLines(screenW/2, 0, screenW/2, screenH, COL_NEON_ORANGE);
    EndScissorMode();

    // UI
    rlViewport(0, 0, renderW, renderH);
    DrawText("P1", 20, 20, 30, COL_NEON_CYAN);
    DrawText(TextFormat("HP: %d", player.hp), 20, 60, 30, COL_NEON_GREEN);

    DrawText("P2", screenW/2 + 20, 20, 30, COL_NEON_ORANGE);
    DrawText(TextFormat("HP: %d", player2.hp), screenW/2 + 20, 60, 30, COL_NEON_GREEN);
    
    DrawLine(screenW/2, 0, screenW/2, screenH, WHITE);
    
    if (current_state == STATE_PVP_RESULT) {
        DrawRectangle(0, screenH/2 - 60, screenW, 120, (Color){0,0,0,220});
        const char* winText = (winner_id == 1) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
        Color winColor = (winner_id == 1) ? COL_NEON_CYAN : COL_NEON_ORANGE;
        DrawText(winText, screenW/2 - MeasureText(winText, 40)/2, screenH/2 - 20, 40, winColor);
        DrawText("PRESS 'R' TO RETURN TITLE", screenW/2 - MeasureText("PRESS 'R' TO RETURN TITLE", 20)/2, screenH/2 + 30, 20, WHITE);
    }
}

void DrawScene(Camera3D cam, bool draw_cursor) {
    // 地面の描画
    DrawCyberGrid(cam.target);

    // マウスカーソル
    if (draw_cursor) {
        Ray ray = GetMouseRay(GetMousePosition(), cam);
        if (ray.direction.y != 0) {
            float t = -ray.position.y / ray.direction.y;
            Vector3 aimPos = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            
            rlPushMatrix();
            rlTranslatef(aimPos.x, 0.1f, aimPos.z);
            rlRotatef(GetTime() * 90.0f, 0, 1, 0);
            
            DrawCubeWires((Vector3){0,0,0}, 2.0f, 0.0f, 2.0f, ColorAlpha(COL_NEON_CYAN, 0.8f));
            DrawCube((Vector3){0,0,0}, 0.3f, 0.3f, 0.3f, WHITE);
            
            rlPopMatrix();
            
            DrawLine3D(player.position, aimPos, ColorAlpha(COL_NEON_CYAN, 0.3f));
        }
    }

    // P1
    Color p1Color = (player.dash_duration > 0) ? COL_NEON_CYAN : BLUE;
    if (player.invincible_timer > 0 && (int)(GetTime()*20)%2 == 0) p1Color = WHITE;
    DrawMecha(player.position, player.facing_angle, p1Color, player.walk_anim_timer, ENEMY_DRONE);
    
    // P1のダッシュの残像
    if(player.dash_duration > 0){
        for(int i=0; i<TRAIL_LENGTH; i+=2) {
            if(player.trail_pos[i].x != 0) {
                Color trailColor = ColorAlpha(COL_NEON_CYAN, 0.3f);
                DrawCube(player.trail_pos[i], 0.8f, 0.8f, 0.8f, trailColor);
            }
        }
    }

    // P2
    if (current_state == STATE_PVP || current_state == STATE_PVP_RESULT || (current_state == STATE_PAUSED && previous_state == STATE_PVP)) {
        Color p2Color = (player2.dash_duration > 0) ? COL_NEON_ORANGE : ORANGE;
        if (player2.invincible_timer > 0 && (int)(GetTime()*20)%2 == 0) p2Color = WHITE;
        DrawMecha(player2.position, player2.facing_angle, p2Color, player2.walk_anim_timer, ENEMY_TANK);
        
        // P2のダッシュの残像
        if(player2.dash_duration > 0){
            for(int i=0; i<TRAIL_LENGTH; i+=2) {
                if(player2.trail_pos[i].x != 0) {
                    Color trailColor = ColorAlpha(COL_NEON_ORANGE, 0.3f);
                    DrawCube(player2.trail_pos[i], 1.2f, 1.2f, 1.2f, trailColor);
                }
            }
        }
    }

    // 敵
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        // 着地点表示
        if (!enemies[i].is_grounded) {
            DrawLine3D(enemies[i].position, (Vector3){enemies[i].position.x, 0, enemies[i].position.z}, ColorAlpha(RED, 0.5f));
            DrawCircle3D((Vector3){enemies[i].position.x, 0.1f, enemies[i].position.z}, 1.0f, (Vector3){1,0,0}, 90, ColorAlpha(RED, 0.3f));
        }

        // 敵の色分け
        Color eColor = COL_NEON_PINK;
        if (enemies[i].type == ENEMY_TANK) eColor = COL_NEON_PURPLE;
        if (enemies[i].type == ENEMY_BOSS) eColor = COL_NEON_ORANGE;
        if (enemies[i].flash_timer > 0) eColor = WHITE;
        DrawMecha(enemies[i].position, 0, eColor, enemies[i].anim_timer, enemies[i].type);
        
        // HPバー
        if (enemies[i].hp < enemies[i].max_hp) {
            Vector3 hpPos = enemies[i].position; 
            float barWidth = (enemies[i].type == ENEMY_BOSS ? 6.0f : 2.0f);
            hpPos.y += (enemies[i].type == ENEMY_BOSS ? 7.0f : 3.0f);
            DrawCube(hpPos, barWidth, 0.3f, 0.2f, BLACK);
            float ratio = (float)enemies[i].hp / (float)enemies[i].max_hp;
            if(ratio < 0) ratio = 0;
            DrawCube(hpPos, barWidth * ratio, 0.35f, 0.25f, COL_NEON_GREEN);
        }
    }
    
    // 弾やアイテムなど
    rlDrawRenderBatchActive(); 
    BeginBlendMode(BLEND_ADDITIVE); 

    // 弾
    for (int i=0; i<MAX_BULLETS; i++) {
        if (bullets[i].active) {
            Color bColor = COL_NEON_CYAN;
            if (bullets[i].is_enemy_bullet) bColor = COL_NEON_PINK;
            if (bullets[i].is_p2_bullet) bColor = COL_NEON_ORANGE;
            float bSize = (bullets[i].is_enemy_bullet || bullets[i].is_p2_bullet) ? 0.6f : 0.4f;
            DrawSphere(bullets[i].position, bSize, bColor);
            DrawSphere(bullets[i].position, bSize * 0.5f, WHITE);
        }
    }

    // アイテム
    for (int i=0; i<MAX_ITEMS; i++) {
        if(items[i].active) {
            rlPushMatrix();
            rlTranslatef(items[i].position.x, 1.0f + sinf(GetTime()*3)*0.2f, items[i].position.z);
            rlRotatef(items[i].angle, 0, 1, 0);
            Color itemColor = (items[i].type == ITEM_HEAL) ? COL_NEON_GREEN : COL_NEON_CYAN;
            DrawCube((Vector3){0,0,0}, 0.8f, 0.8f, 0.8f, itemColor);
            DrawCubeWires((Vector3){0,0,0}, 0.8f, 0.8f, 0.8f, WHITE);
            rlPopMatrix();
        }
    }

    // 爆発
    for (int i=0; i<MAX_PARTICLES; i++) {
        if (particles[i].active) {
            float alpha = particles[i].life / particles[i].max_life;
            Color pColor = ColorAlpha(particles[i].color, alpha);
            DrawCube(particles[i].position, particles[i].size, particles[i].size, particles[i].size, pColor);
        }
    }
    EndBlendMode(); 
}

void SpawnBullet(Vector3 pos, Vector3 direction, bool is_enemy, bool is_p2) {
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].position = (Vector3){pos.x, 1.5f, pos.z};
            float spd = (is_enemy || is_p2) ? 20.0f : 35.0f;
            bullets[i].velocity = Vector3Scale(direction, spd);
            bullets[i].life_time = 2.0f;
            bullets[i].is_enemy_bullet = is_enemy;
            bullets[i].is_p2_bullet = is_p2;
            break;
        }
    }
}

void SpawnEnemy(bool force_boss) {
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].active = true;
            enemies[i].knockback = (Vector3){0,0,0};
            enemies[i].flash_timer = 0; enemies[i].anim_timer = 0;
            enemies[i].shoot_cooldown = 2.0f; enemies[i].attack_range = 20.0f;

            if (force_boss) {
                enemies[i].type = ENEMY_BOSS;
                enemies[i].position = (Vector3){player.position.x, 30.0f, player.position.z + 10.0f}; 
                enemies[i].is_grounded = false; enemies[i].vertical_speed = 0.0f;
                enemies[i].speed = 4.0f + (current_stage * 0.5f);
                enemies[i].max_hp = 300 + (current_stage * 100);
                enemies[i].hp = enemies[i].max_hp;
                boss_spawned = true;
                return;
            }
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float dist = 35.0f;
            bool skyfall = (difficulty == MODE_HARD || current_stage > 2) && GetRandomValue(0, 100) < 40;
            if (skyfall) {
                enemies[i].position = (Vector3){
                    player.position.x + (float)GetRandomValue(-15, 15),
                    25.0f, player.position.z + (float)GetRandomValue(-15, 15)
                };
                enemies[i].is_grounded = false; enemies[i].vertical_speed = 0.0f;
            } else {
                enemies[i].position = (Vector3){ player.position.x + cosf(angle) * dist, 0, player.position.z + sinf(angle) * dist };
                enemies[i].is_grounded = true;
            }
            if (current_stage > 1 && GetRandomValue(0, 100) < 30) {
                enemies[i].type = ENEMY_TANK;
                enemies[i].speed = 3.0f;
                enemies[i].max_hp = 60 + (current_stage * 10);
                enemies[i].hp = enemies[i].max_hp;
                enemies[i].attack_range = 15.0f;
            } else {
                enemies[i].type = ENEMY_DRONE;
                enemies[i].speed = 6.0f;
                enemies[i].max_hp = 20 + (current_stage * 5);
                enemies[i].hp = enemies[i].max_hp;
                enemies[i].attack_range = 5.0f; 
            }
            break;
        }
    }
}

void SpawnItem(Vector3 pos) {
    for (int i=0; i<MAX_ITEMS; i++) {
        if (!items[i].active) {
            items[i].active = true; 
            items[i].position = pos;
            items[i].type = (GetRandomValue(0, 100) < 70) ? ITEM_EXP : ITEM_HEAL; 
            items[i].life_time = 15.0f; 
            items[i].angle = 0;
            break;
        }
    }
}

void SpawnExplosion(Vector3 pos, Color color, int count) {
    int spawned = 0;
    for (int i=0; i<MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].position = pos;
            particles[i].color = color;
            particles[i].max_life = 0.6f;
            particles[i].life = particles[i].max_life;
            particles[i].size = (float)GetRandomValue(3, 8) / 10.0f;
            
            Vector3 rndVec = {
                (float)GetRandomValue(-100, 100),
                (float)GetRandomValue(-100, 100),
                (float)GetRandomValue(-100, 100)
            };
            rndVec = Vector3Normalize(rndVec);
            float speed = (float)GetRandomValue(10, 40) / 2.0f;
            particles[i].velocity = Vector3Scale(rndVec, speed);
            
            spawned++;
            if (spawned >= count) break;
        }
    }
}