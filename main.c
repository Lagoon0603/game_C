#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>

// --- 定数 ---
// 初期のウィンドウサイズ（リサイズ可能）
#define INITIAL_SCREEN_WIDTH 800
#define INITIAL_SCREEN_HEIGHT 450

#define MAX_BULLETS 300
#define MAX_ENEMIES 100
#define MAX_PARTICLES 400
#define MAX_ITEMS 20
#define KILLS_TO_BOSS_BASE 10

// --- ゲームの状態管理 ---
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,      // シングルプレイ
    STATE_PVP,          // 対戦モード
    STATE_BOSS_INTRO,
    STATE_STAGE_CLEAR,
    STATE_GAMEOVER,
    STATE_PVP_RESULT,   // 対戦結果
    STATE_PAUSED        // 一時停止
} GameState;

typedef enum {
    MODE_NORMAL,
    MODE_HARD
} DifficultyMode;

// --- 構造体 ---
typedef struct {
    Vector3 position;
    float speed;
    int hp;
    int max_hp;
    int level;
    int exp;
    int next_level_exp;
    int weapon_type; 
    float shoot_cooldown;
    
    // アクション用
    float dash_cooldown;
    float dash_duration;
    Vector3 dash_dir;
    
    // アニメーション用
    float walk_anim_timer;
    float facing_angle;
    float invincible_timer;
} Player;

typedef enum { ENEMY_DRONE, ENEMY_TANK, ENEMY_BOSS } EnemyType;

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
    
    // 空から降ってくる用
    float vertical_speed; 
    bool is_grounded;     
    
    // 攻撃用
    float shoot_cooldown;
    float attack_range;
} Enemy;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    bool active;
    float life_time;
    bool is_enemy_bullet;
    bool is_p2_bullet; // PvP用
} Bullet;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Color color;
    bool active;
    float life;
    float size;
} Particle;

typedef enum { ITEM_HEAL, ITEM_EXP } ItemType;

typedef struct {
    Vector3 position;
    bool active;
    ItemType type;
    float life_time;
    float angle;
} Item;

// --- グローバル変数 ---
GameState current_state = STATE_TITLE;
GameState previous_state = STATE_TITLE; // ポーズからの復帰用
DifficultyMode difficulty = MODE_NORMAL;

Player player = { 0 };  // P1
Player player2 = { 0 }; // P2 (PvP用)
Enemy enemies[MAX_ENEMIES] = { 0 };
Bullet bullets[MAX_BULLETS] = { 0 };
Particle particles[MAX_PARTICLES] = { 0 };
Item items[MAX_ITEMS] = { 0 };

Camera3D camera = { 0 };   // P1 / シングル用
Camera3D camera2 = { 0 };  // P2用

float game_time = 0.0f;
int winner_id = 0; // 1: P1, 2: P2

// ゲーム進行管理
int current_stage = 1;
int stage_kills = 0;
int kills_required_for_boss = KILLS_TO_BOSS_BASE;
bool boss_spawned = false;
float state_timer = 0.0f;

// --- 関数プロトタイプ ---
void InitGame(bool reset_player);
void UpdateGame();     // シングル用
void UpdateGamePvP();  // PvP用
void UpdatePaused();   // ポーズ画面用
void DrawGame();       // シングル用
void DrawGamePvP();    // PvP用
void DrawPaused();     // ポーズ画面描画
void DrawScene(Camera3D cam); // 3D描画共通部分
void UpdateTitle();
void DrawTitle();
void DrawMecha(Vector3 pos, float angle, Color color, float anim_time, EnemyType type);
void SpawnEnemy(bool force_boss);
void SpawnBullet(Vector3 pos, Vector3 direction, bool is_enemy, bool is_p2);
void SpawnExplosion(Vector3 pos, Color color, int count);
void SpawnItem(Vector3 pos);
void ResetStage();

// --- Main ---
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE); // リサイズ許可
    InitWindow(INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT, "Voxel Survivor 5.3 - Pause & Resizable PvP");
    SetTargetFPS(30);
    
    // ウィンドウをモニター中央に配置
    int monitor = GetCurrentMonitor();
    int x = (GetMonitorWidth(monitor) - INITIAL_SCREEN_WIDTH) / 2;
    int y = (GetMonitorHeight(monitor) - INITIAL_SCREEN_HEIGHT) / 2;
    SetWindowPosition(x, y);

    // カメラ初期設定
    camera.position = (Vector3){ 0.0f, 20.0f, 20.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        // --- 修正部分ここから ---
        // ポーズの切り替え処理を一箇所に集約
        if (IsKeyPressed(KEY_TAB)) {
            if (current_state == STATE_PAUSED) {
                // ポーズ解除
                current_state = previous_state;
            } 
            else if (current_state == STATE_PLAYING || current_state == STATE_PVP || 
                     current_state == STATE_BOSS_INTRO || current_state == STATE_STAGE_CLEAR) {
                // ポーズ開始
                previous_state = current_state;
                current_state = STATE_PAUSED;
            }
        }
        // --- 修正部分ここまで ---

        switch (current_state) {
            case STATE_TITLE:
                UpdateTitle();
                BeginDrawing();
                DrawTitle();
                EndDrawing();
                break;
            case STATE_PVP:
                UpdateGamePvP();
                BeginDrawing();
                DrawGamePvP();
                EndDrawing();
                break;
            case STATE_PAUSED:
                UpdatePaused();
                BeginDrawing();
                // 背景を描画してからポーズUIを重ねる
                if (previous_state == STATE_PVP) DrawGamePvP();
                else DrawGame();
                DrawPaused();
                EndDrawing();
                break;
            case STATE_PVP_RESULT:
                UpdateGamePvP(); // 結果画面でも背景用更新は続ける（入力は受け付けない）
                BeginDrawing();
                DrawGamePvP();
                EndDrawing();
                break;
            default: // PLAYING, BOSS_INTRO, STAGE_CLEAR
                UpdateGame();
                BeginDrawing();
                DrawGame();
                EndDrawing();
                break;
        }
    }
    CloseWindow();
    return 0;
}

// --- ポーズ処理（修正版） ---
void UpdatePaused() {
    // TABキーの処理はmain関数に移したので削除
    
    // タイトルへ戻る
    if (IsKeyPressed(KEY_R)) {
        current_state = STATE_TITLE;
    }
}

void DrawPaused() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    
    // 半透明の黒背景
    DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 150});
    
    DrawText("PAUSED", w/2 - MeasureText("PAUSED", 40)/2, h/2 - 50, 40, WHITE);
    DrawText("PRESS 'TAB' TO RESUME", w/2 - MeasureText("PRESS 'TAB' TO RESUME", 20)/2, h/2 + 10, 20, LIGHTGRAY);
    DrawText("PRESS 'R' TO RETURN TITLE", w/2 - MeasureText("PRESS 'R' TO RETURN TITLE", 20)/2, h/2 + 40, 20, LIGHTGRAY);
}

// --- タイトル処理 ---
void UpdateTitle() {
    float time = GetTime();
    camera.position.x = sinf(time * 0.5f) * 30.0f;
    camera.position.z = cosf(time * 0.5f) * 30.0f;
    camera.target = (Vector3){ 0, 0, 0 };

    if (IsKeyDown(KEY_N)) {
        difficulty = MODE_NORMAL;
        InitGame(true);
        current_state = STATE_PLAYING;
    }
    if (IsKeyDown(KEY_H)) {
        difficulty = MODE_HARD;
        InitGame(true);
        current_state = STATE_PLAYING;
    }
    // PvPモード起動
    if (IsKeyDown(KEY_P)) {
        InitGame(true);
        player.position = (Vector3){ -10, 0, 0 };
        player2.position = (Vector3){ 10, 0, 0 };
        player2.hp = 100;
        player2.max_hp = 100;
        player2.speed = 10.0f;
        camera2 = camera;
        current_state = STATE_PVP;
    }
}

void DrawTitle() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    ClearBackground((Color){10, 10, 20, 255});
    BeginMode3D(camera);
        DrawGrid(100, 2.0f);
        DrawMecha((Vector3){5,0,0}, 0, RED, GetTime(), ENEMY_DRONE);
        DrawMecha((Vector3){-5,0,0}, 3.14, DARKPURPLE, GetTime(), ENEMY_TANK);
        DrawMecha((Vector3){0,0,-10}, 0, ORANGE, GetTime(), ENEMY_BOSS);
    EndMode3D();

    DrawText("VOXEL SURVIVOR", w/2 - MeasureText("VOXEL SURVIVOR", 60)/2, 100, 60, SKYBLUE);
    DrawText("BOSS & PVP UPDATE", w/2 - MeasureText("BOSS & PVP UPDATE", 30)/2, 170, 30, GREEN);

    DrawText("[N] NORMAL MODE", w/2 - 150, 300, 40, WHITE);
    DrawText("[H] HARD MODE", w/2 - 150, 350, 40, RED);
    DrawText("[P] VS 2P MODE", w/2 - 150, 400, 40, YELLOW);
    
    DrawText("P1: WASD+Mouse / P2: Arrows+RShift+Enter", w/2 - MeasureText("P1: WASD+Mouse / P2: Arrows+RShift+Enter", 20)/2, 550, 20, DARKGRAY);
}

// --- ゲーム初期化 ---
void InitGame(bool reset_player) {
    if (reset_player) {
        player.position = (Vector3){ 0, 0, 0 };
        player.speed = 10.0f;
        player.hp = 100;
        player.max_hp = 100;
        player.level = 1;
        player.exp = 0;
        player.next_level_exp = 10;
        player.weapon_type = 0;
        player.dash_cooldown = 0;
        player.dash_duration = 0;
        player.invincible_timer = 0;
        current_stage = 1;
        player2 = player; // P2初期化
    }
    ResetStage();
    camera.position = (Vector3){ 0.0f, 25.0f, 18.0f }; 
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 50.0f;
    game_time = 0.0f;
}

void ResetStage() {
    stage_kills = 0;
    boss_spawned = false;
    kills_required_for_boss = KILLS_TO_BOSS_BASE + (current_stage - 1) * 5; 
    for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = false;
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
    for(int i=0; i<MAX_PARTICLES; i++) particles[i].active = false;
    for(int i=0; i<MAX_ITEMS; i++) items[i].active = false;
}

// --- メカ描画 ---
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
    DrawCubeWires((Vector3){0, bodySize, 0}, bodySize, bodySize, bodySize, BLACK);

    Vector3 headPos = {0, bodySize * 1.8f, 0};
    float headSize = bodySize * 0.6f;
    DrawCube(headPos, headSize, headSize, headSize, GRAY);
    DrawCube((Vector3){0, headPos.y, headSize/2 + 0.05f}, headSize*0.8f, headSize*0.3f, 0.1f, SKYBLUE);

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
        DrawCube((Vector3){0.8f, 0.5f, 0.2f}, 0.2f, 0.2f, 1.0f, RED);
        DrawCube((Vector3){-0.8f, 0.5f, 0.2f}, 0.2f, 0.2f, 1.0f, RED);
        rlPopMatrix();
    }
    rlPopMatrix();
}

// --- シングルプレイ更新 ---
void UpdateGame() {
    float dt = GetFrameTime();
    game_time += dt;

    if (current_state == STATE_GAMEOVER) {
        if (IsKeyPressed(KEY_R)) current_state = STATE_TITLE;
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
        state_timer += dt;
        if (state_timer > 2.0f) {
            SpawnEnemy(true);
            current_state = STATE_PLAYING;
        }
        return; 
    }

    // カメラ
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    float t = -ray.position.y / ray.direction.y;
    Vector3 aim_point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    
    Vector3 d = Vector3Subtract(aim_point, player.position);
    player.facing_angle = -atan2f(d.z, d.x) + PI/2;
    
    Vector3 mouseOffset = Vector3Scale(Vector3Normalize(Vector3Subtract(aim_point, player.position)), 5.0f);
    Vector3 targetCamPos = {
        player.position.x + mouseOffset.x * 0.3f, 
        25.0f, 
        player.position.z + 18.0f + mouseOffset.z * 0.3f
    };
    camera.position = Vector3Lerp(camera.position, targetCamPos, 0.08f);
    camera.target = Vector3Lerp(camera.target, player.position, 0.1f);

    // プレイヤー処理
    if (player.invincible_timer > 0) player.invincible_timer -= dt;
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT)) && player.dash_cooldown <= 0) {
        player.dash_duration = 0.2f;
        player.dash_cooldown = 1.5f;
        Vector3 input = {0};
        if (IsKeyDown(KEY_W)) input.z -= 1;
        if (IsKeyDown(KEY_S)) input.z += 1;
        if (IsKeyDown(KEY_A)) input.x -= 1;
        if (IsKeyDown(KEY_D)) input.x += 1;
        if (Vector3Length(input) > 0) player.dash_dir = Vector3Normalize(input);
        else {
            Vector3 aim = Vector3Subtract(aim_point, player.position);
            aim.y = 0;
            player.dash_dir = Vector3Normalize(aim);
        }
        SpawnExplosion(player.position, WHITE, 5);
    }
    if (player.dash_duration > 0) {
        player.dash_duration -= dt;
        player.position = Vector3Add(player.position, Vector3Scale(player.dash_dir, player.speed * 3.0f * dt));
        SpawnExplosion(player.position, SKYBLUE, 1);
    } else {
        Vector3 move = {0};
        if (IsKeyDown(KEY_W)) move.z -= 1;
        if (IsKeyDown(KEY_S)) move.z += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;
        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
            player.position = Vector3Add(player.position, Vector3Scale(move, player.speed * dt));
            player.walk_anim_timer += dt;
        } else player.walk_anim_timer = 0;
    }
    if (player.dash_cooldown > 0) player.dash_cooldown -= dt;
    
    if (player.position.x > 50) player.position.x = 50;
    if (player.position.x < -50) player.position.x = -50;
    if (player.position.z > 50) player.position.z = 50;
    if (player.position.z < -50) player.position.z = -50;

    if (player.shoot_cooldown > 0) player.shoot_cooldown -= dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.shoot_cooldown <= 0) {
        Vector3 aim_dir = Vector3Normalize(Vector3Subtract(aim_point, player.position));
        aim_dir.y = 0;
        SpawnBullet(player.position, aim_dir, false, false);
        player.shoot_cooldown = 0.2f;
    }

    // 弾更新
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector3Add(bullets[i].position, Vector3Scale(bullets[i].velocity, dt));
        bullets[i].life_time -= dt;
        if (bullets[i].life_time <= 0 || fabs(bullets[i].position.x) > 60 || fabs(bullets[i].position.z) > 60) {
            bullets[i].active = false;
            continue;
        }
        if (bullets[i].is_enemy_bullet && player.invincible_timer <= 0 && player.dash_duration <= 0) {
            Vector3 playerCenter = { player.position.x, 1.0f, player.position.z };
            if (Vector3Distance(bullets[i].position, playerCenter) < 2.0f) { 
                player.hp -= 10;
                player.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player.position, RED, 10);
                if (player.hp <= 0) current_state = STATE_GAMEOVER;
            }
        }
    }

    // アイテム
    for (int i=0; i<MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        items[i].angle += dt * 90.0f;
        items[i].life_time -= dt;
        if (items[i].life_time <= 0) items[i].active = false;
        if (Vector3Distance(player.position, items[i].position) < 2.0f) {
            player.hp += 30;
            if(player.hp > player.max_hp) player.hp = player.max_hp;
            SpawnExplosion(player.position, GREEN, 5);
            items[i].active = false;
        }
    }

    // 敵スポーン
    if (!boss_spawned) {
        if (stage_kills >= kills_required_for_boss) {
            current_state = STATE_BOSS_INTRO;
            state_timer = 0.0f;
            for(int i=0; i<MAX_ENEMIES; i++) if(enemies[i].active) {
                enemies[i].hp = 0;
                SpawnExplosion(enemies[i].position, RED, 5);
                enemies[i].active = false;
            }
        } else {
            float spawnRate = 2.0f + (current_stage * 0.5f);
            if (difficulty == MODE_HARD) spawnRate *= 1.5f;
            if (GetRandomValue(0, 100) < (int)spawnRate) SpawnEnemy(false);
        }
    }

    // 敵更新
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
            if (player.hp <= 0) current_state = STATE_GAMEOVER;
        }

        float hitSize = (enemies[i].type == ENEMY_BOSS) ? 2.5f : 1.0f;
        BoundingBox box = {
            (Vector3){enemies[i].position.x - hitSize, 0, enemies[i].position.z - hitSize},
            (Vector3){enemies[i].position.x + hitSize, hitSize * 2.5f, enemies[i].position.z + hitSize}
        };
        for (int b=0; b<MAX_BULLETS; b++) {
            if (!bullets[b].active || bullets[b].is_enemy_bullet) continue;
            if (CheckCollisionBoxSphere(box, bullets[b].position, 0.3f)) {
                bullets[b].active = false;
                enemies[i].hp -= 10;
                enemies[i].flash_timer = 0.1f;
                if (enemies[i].type != ENEMY_BOSS) {
                    Vector3 push = Vector3Normalize(bullets[b].velocity);
                    enemies[i].knockback = Vector3Add(enemies[i].knockback, Vector3Scale(push, 15.0f));
                }
                SpawnExplosion(bullets[b].position, YELLOW, 2);
                if (enemies[i].hp <= 0) {
                    enemies[i].active = false;
                    SpawnExplosion(enemies[i].position, ORANGE, 15);
                    if (enemies[i].type == ENEMY_BOSS) {
                        boss_spawned = false; 
                        current_state = STATE_STAGE_CLEAR;
                        state_timer = 0;
                    } else {
                        stage_kills++;
                        if (GetRandomValue(0, 100) < 10) SpawnItem(enemies[i].position);
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

// --- PvP 更新 ---
void UpdateGamePvP() {
    float dt = GetFrameTime();
    if (current_state == STATE_PVP_RESULT) {
        if (IsKeyPressed(KEY_R)) current_state = STATE_TITLE;
        return;
    }

    // P1
    if (player.invincible_timer > 0) player.invincible_timer -= dt;
    if (IsKeyPressed(KEY_SPACE) && player.dash_cooldown <= 0) {
        player.dash_duration = 0.2f;
        player.dash_cooldown = 1.5f;
        Vector3 input = {0};
        if (IsKeyDown(KEY_W)) input.z -= 1;
        if (IsKeyDown(KEY_S)) input.z += 1;
        if (IsKeyDown(KEY_A)) input.x -= 1;
        if (IsKeyDown(KEY_D)) input.x += 1;
        if (Vector3Length(input) > 0) player.dash_dir = Vector3Normalize(input);
        else player.dash_dir = (Vector3){0,0,-1};
    }
    if (player.dash_duration > 0) {
        player.dash_duration -= dt;
        player.position = Vector3Add(player.position, Vector3Scale(player.dash_dir, player.speed * 3.0f * dt));
    } else {
        Vector3 move = {0};
        if (IsKeyDown(KEY_W)) move.z -= 1;
        if (IsKeyDown(KEY_S)) move.z += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;
        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
            player.position = Vector3Add(player.position, Vector3Scale(move, player.speed * dt));
            player.walk_anim_timer += dt;
        } else player.walk_anim_timer = 0;
    }
    if (player.dash_cooldown > 0) player.dash_cooldown -= dt;

    // --- P1 照準 & 射撃 (マウス操作: 左画面補正あり) ---
    Vector2 mousePos = GetMousePosition();
    float screenW = (float)GetScreenWidth();

    // 1. マウス座標を左画面内(0 ~ 半分)に制限する
    //    (マウスが右画面に行っても、左画面の右端として扱います)
    if (mousePos.x > screenW / 2.0f) mousePos.x = screenW / 2.0f;

    // 2. 左画面の座標(0 ~ W/2) を 仮想的な全画面座標(0 ~ W) に引き伸ばす
    //    これで「左画面の中央」が「カメラの正面」として正しく計算されます
    mousePos.x *= 2.0f;

    // 補正したマウス座標を使ってレイを飛ばす
    Ray ray = GetMouseRay(mousePos, camera);
    
    float t = -ray.position.y / ray.direction.y;
    Vector3 aim_point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    
    Vector3 d = Vector3Subtract(aim_point, player.position);
    d.y = 0; // 高さは無視
    
    player.facing_angle = -atan2f(d.z, d.x) + PI/2;
    
    if (player.shoot_cooldown > 0) player.shoot_cooldown -= dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.shoot_cooldown <= 0) {
        SpawnBullet(player.position, Vector3Normalize(d), false, false);
        player.shoot_cooldown = 0.3f;
    }

    // P2
    if (player2.invincible_timer > 0) player2.invincible_timer -= dt;
    if (IsKeyPressed(KEY_ENTER) && player2.dash_cooldown <= 0) {
        player2.dash_duration = 0.2f;
        player2.dash_cooldown = 1.5f;
        Vector3 input = {0};
        if (IsKeyDown(KEY_UP)) input.z -= 1;
        if (IsKeyDown(KEY_DOWN)) input.z += 1;
        if (IsKeyDown(KEY_LEFT)) input.x -= 1;
        if (IsKeyDown(KEY_RIGHT)) input.x += 1;
        if (Vector3Length(input) > 0) player2.dash_dir = Vector3Normalize(input);
        else player2.dash_dir = (Vector3){0,0,1};
    }
    if (player2.dash_duration > 0) {
        player2.dash_duration -= dt;
        player2.position = Vector3Add(player2.position, Vector3Scale(player2.dash_dir, player2.speed * 3.0f * dt));
    } else {
        Vector3 move = {0};
        if (IsKeyDown(KEY_UP)) move.z -= 1;
        if (IsKeyDown(KEY_DOWN)) move.z += 1;
        if (IsKeyDown(KEY_LEFT)) move.x -= 1;
        if (IsKeyDown(KEY_RIGHT)) move.x += 1;
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

    float limit = 40.0f;
    if (player.position.x > limit) player.position.x = limit; if (player.position.x < -limit) player.position.x = -limit;
    if (player.position.z > limit) player.position.z = limit; if (player.position.z < -limit) player.position.z = -limit;
    if (player2.position.x > limit) player2.position.x = limit; if (player2.position.x < -limit) player2.position.x = -limit;
    if (player2.position.z > limit) player2.position.z = limit; if (player2.position.z < -limit) player2.position.z = -limit;

    camera.target = player.position;
    camera.position = Vector3Add(player.position, (Vector3){0, 20, 15});
    camera2.target = player2.position;
    camera2.position = Vector3Add(player2.position, (Vector3){0, 20, 15});

    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector3Add(bullets[i].position, Vector3Scale(bullets[i].velocity, dt));
        bullets[i].life_time -= dt;
        if (bullets[i].life_time <= 0) { bullets[i].active = false; continue; }

        Vector3 p1Center = {player.position.x, 1, player.position.z};
        Vector3 p2Center = {player2.position.x, 1, player2.position.z};

        if (bullets[i].is_p2_bullet && player.invincible_timer <= 0 && player.dash_duration <= 0) {
            if (Vector3Distance(bullets[i].position, p1Center) < 2.0f) {
                player.hp -= 5;
                player.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player.position, RED, 10);
                if (player.hp <= 0) { current_state = STATE_PVP_RESULT; winner_id = 2; }
            }
        }
        else if (!bullets[i].is_p2_bullet && player2.invincible_timer <= 0 && player2.dash_duration <= 0) {
            if (Vector3Distance(bullets[i].position, p2Center) < 2.0f) {
                player2.hp -= 5;
                player2.invincible_timer = 0.5f;
                bullets[i].active = false;
                SpawnExplosion(player2.position, RED, 10);
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

// --- 描画 (シングル) ---
void DrawGame() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    ClearBackground((Color){15, 15, 20, 255});
    BeginMode3D(camera);
    DrawScene(camera);
    EndMode3D();

    DrawText(TextFormat("STAGE %d", current_stage), 20, 20, 30, WHITE);
    DrawText(TextFormat("HP: %d/%d", player.hp, player.max_hp), 20, 60, 40, (player.hp < 30 ? RED : GREEN));
    
    if (!boss_spawned) {
        float progress = (float)stage_kills / kills_required_for_boss;
        if(progress > 1.0) progress = 1.0;
        DrawRectangle(w/2 - 150, 50, 300, 20, DARKGRAY);
        DrawRectangle(w/2 - 150, 50, 300 * progress, 20, ORANGE);
    } else DrawText("!! BOSS ACTIVE !!", w/2 - 100, 30, 30, RED);

    if (current_state == STATE_BOSS_INTRO) {
        DrawRectangle(0, h/2 - 60, w, 120, (Color){0,0,0,150});
        DrawText("WARNING", w/2 - MeasureText("WARNING", 50)/2, h/2 - 40, 50, RED);
    }
    if (current_state == STATE_STAGE_CLEAR) {
        DrawRectangle(0, h/2 - 60, w, 120, (Color){255,255,255,150});
        DrawText("STAGE CLEAR!", w/2 - MeasureText("STAGE CLEAR!", 50)/2, h/2 - 20, 50, GOLD);
    }
    if (current_state == STATE_GAMEOVER) {
        DrawRectangle(0, 0, w, h, (Color){0,0,0,200});
        DrawText("GAME OVER", w/2 - MeasureText("GAME OVER", 80)/2, h/2 - 50, 80, RED);
        DrawText("PRESS 'R' TO RETURN TITLE", w/2 - MeasureText("PRESS 'R' TO RETURN TITLE", 20)/2, h/2 + 50, 20, GRAY);
    }
}

// --- 描画 (PvP - Retina/HighDPI対応版) ---
void DrawGamePvP() {
    // UIやシザー用（論理サイズ）
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    
    // ビューポート用（物理サイズ：Retinaなどで値が大きくなる）
    int renderW = GetRenderWidth();
    int renderH = GetRenderHeight();
    
    ClearBackground(BLACK);
    
    // --- 左画面 (P1) ---
    // ビューポート設定：物理ピクセル基準
    rlViewport(0, 0, renderW/2, renderH); 
    // シザー設定：論理ピクセル基準（はみ出し防止）
    BeginScissorMode(0, 0, screenW/2, screenH);
        ClearBackground((Color){20, 20, 30, 255});
        BeginMode3D(camera);
            DrawScene(camera);
        EndMode3D();
    EndScissorMode();

    // --- 右画面 (P2) ---
    // 開始位置Xも物理ピクセル基準で指定
    rlViewport(renderW/2, 0, renderW/2, renderH);
    BeginScissorMode(screenW/2, 0, screenW/2, screenH);
        ClearBackground((Color){30, 20, 20, 255});
        BeginMode3D(camera2);
            DrawScene(camera2);
        EndMode3D();
    EndScissorMode();

    // --- UI (全画面に戻す) ---
    // ビューポートを物理サイズの全画面に戻す
    rlViewport(0, 0, renderW, renderH);
    
    // UI描画は論理座標(screenW, screenH)で行う
    DrawText("P1 (WASD)", 20, 20, 20, SKYBLUE);
    DrawText(TextFormat("HP: %d", player.hp), 20, 50, 30, GREEN);

    DrawText("P2 (ARROWS)", screenW/2 + 20, 20, 20, ORANGE);
    DrawText(TextFormat("HP: %d", player2.hp), screenW/2 + 20, 50, 30, GREEN);
    
    DrawLine(screenW/2, 0, screenW/2, screenH, WHITE);
    
    if (current_state == STATE_PVP_RESULT) {
        DrawRectangle(0, screenH/2 - 60, screenW, 120, (Color){0,0,0,200});
        const char* winText = (winner_id == 1) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
        Color winColor = (winner_id == 1) ? SKYBLUE : ORANGE;
        DrawText(winText, screenW/2 - MeasureText(winText, 40)/2, screenH/2 - 20, 40, winColor);
        DrawText("PRESS 'R' TO RETURN TITLE", screenW/2 - MeasureText("PRESS 'R' TO RETURN TITLE", 20)/2, screenH/2 + 30, 20, WHITE);
    }
}

// --- 3D描画共通 ---
void DrawScene(Camera3D cam) {
    rlPushMatrix();
    rlTranslatef(0, -0.1f, 0);
    DrawGrid(100, 2.0f); 
    rlPopMatrix();

    Color p1Color = (player.dash_duration > 0) ? SKYBLUE : BLUE;
    if (player.invincible_timer > 0 && (int)(GetTime()*20)%2 == 0) p1Color = WHITE;
    DrawMecha(player.position, player.facing_angle, p1Color, player.walk_anim_timer, ENEMY_DRONE);
    
    if (current_state == STATE_PVP || current_state == STATE_PVP_RESULT || (current_state == STATE_PAUSED && previous_state == STATE_PVP)) {
        Color p2Color = (player2.dash_duration > 0) ? YELLOW : ORANGE;
        if (player2.invincible_timer > 0 && (int)(GetTime()*20)%2 == 0) p2Color = WHITE;
        DrawMecha(player2.position, player2.facing_angle, p2Color, player2.walk_anim_timer, ENEMY_TANK);
    }

    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        if (!enemies[i].is_grounded) {
            DrawCylinder((Vector3){enemies[i].position.x, 0, enemies[i].position.z}, 1.0f, 1.0f, 0.1f, 10, (Color){0,0,0,100});
        }
        Color eColor = RED;
        if (enemies[i].type == ENEMY_TANK) eColor = DARKPURPLE;
        if (enemies[i].type == ENEMY_BOSS) eColor = ORANGE;
        if (enemies[i].flash_timer > 0) eColor = WHITE;
        DrawMecha(enemies[i].position, 0, eColor, enemies[i].anim_timer, enemies[i].type);
        
        bool showBar = (enemies[i].hp < enemies[i].max_hp) || (enemies[i].type == ENEMY_BOSS);
        if (showBar) {
            Vector3 hpPos = enemies[i].position; 
            float barWidth = (enemies[i].type == ENEMY_BOSS ? 6.0f : 2.0f);
            hpPos.y += (enemies[i].type == ENEMY_BOSS ? 7.0f : 3.0f);
            DrawCube(hpPos, barWidth, 0.3f, 0.2f, BLACK);
            float ratio = (float)enemies[i].hp / (float)enemies[i].max_hp;
            if(ratio < 0) ratio = 0;
            DrawCube(hpPos, barWidth * ratio, 0.35f, 0.25f, GREEN);
        }
    }
    
    for (int i=0; i<MAX_BULLETS; i++) {
        if (bullets[i].active) {
            Color bColor = YELLOW;
            if (bullets[i].is_enemy_bullet) bColor = PINK;
            if (bullets[i].is_p2_bullet) bColor = ORANGE;
            float bSize = (bullets[i].is_enemy_bullet || bullets[i].is_p2_bullet) ? 0.5f : 0.3f;
            DrawSphere(bullets[i].position, bSize, bColor);
        }
    }
    for (int i=0; i<MAX_ITEMS; i++) {
        if(items[i].active) {
            rlPushMatrix();
            rlTranslatef(items[i].position.x, 1.0f + sinf(GetTime()*3)*0.2f, items[i].position.z);
            rlRotatef(items[i].angle, 0, 1, 0);
            DrawCube((Vector3){0,0,0}, 0.8f, 0.8f, 0.8f, GREEN);
            DrawCubeWires((Vector3){0,0,0}, 0.8f, 0.8f, 0.8f, WHITE);
            rlPopMatrix();
        }
    }
    for (int i=0; i<MAX_PARTICLES; i++) {
        if (particles[i].active) DrawCube(particles[i].position, particles[i].size, particles[i].size, particles[i].size, particles[i].color);
    }
}

void SpawnBullet(Vector3 pos, Vector3 direction, bool is_enemy, bool is_p2) {
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].position = (Vector3){pos.x, 1.5f, pos.z};
            float spd = (is_enemy || is_p2) ? 20.0f : 30.0f;
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
            enemies[i].flash_timer = 0;
            enemies[i].anim_timer = 0;
            enemies[i].shoot_cooldown = 2.0f;
            enemies[i].attack_range = 20.0f;

            if (force_boss) {
                enemies[i].type = ENEMY_BOSS;
                enemies[i].position = (Vector3){player.position.x, 30.0f, player.position.z + 10.0f}; 
                enemies[i].is_grounded = false;
                enemies[i].vertical_speed = 0.0f;
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
                enemies[i].is_grounded = false;
                enemies[i].vertical_speed = 0.0f;
            } else {
                enemies[i].position = (Vector3){
                    player.position.x + cosf(angle) * dist, 0, player.position.z + sinf(angle) * dist
                };
                enemies[i].is_grounded = true;
            }
            if (current_stage > 1 && GetRandomValue(0, 100) < 30) {
                enemies[i].type = ENEMY_TANK;
                enemies[i].speed = 3.5f + (current_stage * 0.2f);
                enemies[i].max_hp = 80 + (current_stage * 20);
                enemies[i].hp = enemies[i].max_hp;
                enemies[i].attack_range = 15.0f;
            } else {
                enemies[i].type = ENEMY_DRONE;
                enemies[i].speed = 8.0f + (current_stage * 0.5f);
                enemies[i].max_hp = 30 + (current_stage * 10);
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
            items[i].type = ITEM_HEAL; 
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
            particles[i].life = 0.4f;
            particles[i].size = (float)GetRandomValue(2, 5) / 10.0f;
            particles[i].velocity = (Vector3){
                (float)GetRandomValue(-50, 50)/5.0f,
                (float)GetRandomValue(20, 80)/5.0f,
                (float)GetRandomValue(-50, 50)/5.0f
            };
            spawned++;
            if (spawned >= count) break;
        }
    }
}