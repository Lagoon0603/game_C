#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>

// --- 定数 ---
// #define SCREEN_WIDTH 1280
// #define SCREEN_HEIGHT 720
// リモート環境での動作安定化のため解像度を縮小
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define MAX_BULLETS 200
#define MAX_ENEMIES 80
#define MAX_PARTICLES 400

// --- ゲームの状態管理 ---
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_GAMEOVER
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
} Player;

typedef enum { ENEMY_DRONE, ENEMY_TANK } EnemyType;

typedef struct {
    Vector3 position;
    bool active;
    EnemyType type;
    float speed;
    float hp;
    float max_hp;
    Vector3 knockback;
    float flash_timer;
    float anim_timer;
    
    // 空から降ってくる用
    float vertical_speed; // 落下速度
    bool is_grounded;     // 着地したか？
} Enemy;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    bool active;
    float life_time;
} Bullet;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Color color;
    bool active;
    float life;
    float size;
} Particle;

// --- グローバル変数 ---
GameState current_state = STATE_TITLE;
DifficultyMode difficulty = MODE_NORMAL;

Player player = { 0 };
Enemy enemies[MAX_ENEMIES] = { 0 };
Bullet bullets[MAX_BULLETS] = { 0 };
Particle particles[MAX_PARTICLES] = { 0 };
Camera3D camera = { 0 };
float game_time = 0.0f;

// --- 関数プロトタイプ ---
void InitGame();
void UpdateGame();
void DrawGame();
void UpdateTitle();
void DrawTitle();
void DrawMecha(Vector3 pos, float angle, Color color, float anim_time, bool is_tank);
void SpawnEnemy();
void SpawnBullet(Vector3 direction);
void SpawnExplosion(Vector3 pos, Color color, int count);

// --- Main ---
int main(void) {
    // リモート環境向け設定: ウィンドウリサイズ許可
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Voxel Survivor 4.0 - Skyfall");
    
    // リモート環境では60FPSの転送が厳しいため30FPSに制限して安定化
    SetTargetFPS(30);
    
    // 最初のカメラ初期化（タイトル用）
    camera.position = (Vector3){ 0.0f, 20.0f, 20.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        if (current_state == STATE_TITLE) {
            UpdateTitle();
            BeginDrawing();
            DrawTitle();
            EndDrawing();
        } else {
            UpdateGame();
            BeginDrawing();
            DrawGame();
            EndDrawing();
        }
    }
    CloseWindow();
    return 0;
}

// --- タイトル画面処理 ---
void UpdateTitle() {
    // カメラをゆっくり回転させる演出
    float time = GetTime();
    camera.position.x = sinf(time * 0.5f) * 30.0f;
    camera.position.z = cosf(time * 0.5f) * 30.0f;
    camera.target = (Vector3){ 0, 0, 0 };

    if (IsKeyPressed(KEY_N)) {
        difficulty = MODE_NORMAL;
        InitGame();
        current_state = STATE_PLAYING;
    }
    if (IsKeyPressed(KEY_H)) {
        difficulty = MODE_HARD;
        InitGame();
        current_state = STATE_PLAYING;
    }
}

void DrawTitle() {
    ClearBackground((Color){10, 10, 20, 255});
    
    BeginMode3D(camera);
        DrawGrid(100, 2.0f);
        // タイトル画面用のデモ敵
        DrawMecha((Vector3){5,0,0}, 0, RED, GetTime(), false);
        DrawMecha((Vector3){-5,0,0}, 3.14, DARKPURPLE, GetTime(), true);
    EndMode3D();

    DrawText("VOXEL SURVIVOR", SCREEN_WIDTH/2 - 300, 150, 60, SKYBLUE);
    DrawText("3D MECHA ACTION", SCREEN_WIDTH/2 - 200, 220, 30, GRAY);

    DrawText("[N] NORMAL MODE", SCREEN_WIDTH/2 - 150, 400, 40, WHITE);
    DrawText("[H] HARD MODE (SKYFALL)", SCREEN_WIDTH/2 - 220, 460, 40, RED);
    
    DrawText("WASD: Move / MOUSE: Aim / CLICK: Shot / SPACE: Dash", SCREEN_WIDTH/2 - 300, 600, 20, DARKGRAY);
}

// --- ゲーム本体初期化 ---
void InitGame() {
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

    // カメラ設定（プレイ用：少し低くして奥行きを強調）
    camera.position = (Vector3){ 0.0f, 22.0f, 16.0f }; 
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 50.0f;
    
    game_time = 0.0f;
    
    // 配列クリア
    for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = false;
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
    for(int i=0; i<MAX_PARTICLES; i++) particles[i].active = false;
}

// --- ロボット描画 ---
void DrawMecha(Vector3 pos, float angle, Color color, float anim_time, bool is_tank) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(angle * RAD2DEG, 0, 1, 0);
    
    float bounce = sinf(anim_time * 15.0f) * 0.1f;
    if (is_tank) bounce *= 0.2f;
    rlTranslatef(0, bounce, 0);

    float bodySize = is_tank ? 1.5f : 0.8f;
    DrawCube((Vector3){0, bodySize, 0}, bodySize, bodySize, bodySize, color);
    DrawCubeWires((Vector3){0, bodySize, 0}, bodySize, bodySize, bodySize, BLACK);

    Vector3 headPos = {0, bodySize * 1.8f, 0};
    float headSize = bodySize * 0.6f;
    DrawCube(headPos, headSize, headSize, headSize, GRAY);
    DrawCube((Vector3){0, headPos.y, headSize/2 + 0.05f}, headSize*0.8f, headSize*0.3f, 0.1f, SKYBLUE);

    float legOffset = bodySize * 0.4f;
    float legLength = is_tank ? 0.8f : 1.0f;
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
    
    if (!is_tank) {
        rlPushMatrix();
        rlTranslatef(bodySize/2 + 0.2f, bodySize, 0.3f);
        DrawCube((Vector3){0, 0, 0.5f}, 0.2f, 0.2f, 1.0f,BLACK);
        rlPopMatrix();
    }
    rlPopMatrix();
}

void UpdateGame() {
    if (current_state == STATE_GAMEOVER) {
        if (IsKeyPressed(KEY_R)) current_state = STATE_TITLE;
        return;
    }

    float dt = GetFrameTime();
    game_time += dt;

    // --- マウスレイキャスティング ---
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    float t = -ray.position.y / ray.direction.y;
    Vector3 aim_point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    
    Vector3 d = Vector3Subtract(aim_point, player.position);
    player.facing_angle = -atan2f(d.z, d.x) + PI/2;

    // --- プレイヤー操作 ---
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT)) && player.dash_cooldown <= 0) {
        player.dash_duration = 0.2f;
        player.dash_cooldown = 2.0f;
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
        } else {
            player.walk_anim_timer = 0;
        }
    }
    
    if (player.dash_cooldown > 0) player.dash_cooldown -= dt;

    // --- カメラ制御（改良版） ---
    // プレイヤーの位置 + マウスの方向に少しずらす（覗き込み）
    Vector3 mouseOffset = Vector3Scale(Vector3Normalize(Vector3Subtract(aim_point, player.position)), 5.0f);
    Vector3 targetCamPos = {
        player.position.x + mouseOffset.x * 0.3f, 
        22.0f, // 高さ
        player.position.z + 18.0f + mouseOffset.z * 0.3f // 奥行き
    };
    // カメラ位置を滑らかに移動 (Lerp)
    camera.position = Vector3Lerp(camera.position, targetCamPos, 0.08f);
    camera.target = Vector3Lerp(camera.target, player.position, 0.1f);


    // --- 射撃 ---
    if (player.shoot_cooldown > 0) player.shoot_cooldown -= dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && player.shoot_cooldown <= 0) {
        Vector3 aim_dir = Vector3Normalize(Vector3Subtract(aim_point, player.position));
        aim_dir.y = 0;
        
        if (player.weapon_type == 0) {
            SpawnBullet(aim_dir);
            player.shoot_cooldown = 0.2f;
        } else if (player.weapon_type >= 1) {
             float spread = (float)GetRandomValue(-15, 15) * DEG2RAD;
             Vector3 sDir = {
                 aim_dir.x * cosf(spread) - aim_dir.z * sinf(spread), 0,
                 aim_dir.x * sinf(spread) + aim_dir.z * cosf(spread)
             };
            SpawnBullet(sDir);
            player.shoot_cooldown = 0.08f;
        }
    }

    // --- 弾更新 ---
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].position = Vector3Add(bullets[i].position, Vector3Scale(bullets[i].velocity, dt));
        bullets[i].life_time -= dt;
        if (bullets[i].life_time <= 0) bullets[i].active = false;
    }

    // --- 敵のスポーン制御 ---
    // 難易度係数
    float spawnRate = 2.0f + (game_time/10.0f);
    if (difficulty == MODE_HARD) spawnRate *= 1.5f; // ハードは湧きが多い

    if (GetRandomValue(0, 100) < (int)spawnRate) SpawnEnemy();

    // --- 敵更新 ---
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        
        // 落下処理（ハードモード用）
        if (!enemies[i].is_grounded) {
            enemies[i].vertical_speed -= 40.0f * dt; // 重力
            enemies[i].position.y += enemies[i].vertical_speed * dt;
            
            // 着地判定
            if (enemies[i].position.y <= 0) {
                enemies[i].position.y = 0;
                enemies[i].is_grounded = true;
                SpawnExplosion(enemies[i].position, LIGHTGRAY, 5); // 着地エフェクト（砂煙）
                
                // 着地した瞬間、プレイヤーが近くにいたらダメージ（プレス攻撃）
                if (Vector3Distance(enemies[i].position, player.position) < 3.0f && player.dash_duration <= 0) {
                     player.hp -= 10;
                     SpawnExplosion(player.position, RED, 10);
                }
            } else {
                continue; // 空中にいる間は移動しない
            }
        }
        
        // 地上での行動
        Vector3 to_player = Vector3Subtract(player.position, enemies[i].position);
        float dist = Vector3Length(to_player);
        to_player = Vector3Normalize(to_player);
        
        enemies[i].anim_timer += dt;

        Vector3 move = Vector3Scale(to_player, enemies[i].speed * dt);
        Vector3 knock = Vector3Scale(enemies[i].knockback, dt);
        enemies[i].position = Vector3Add(enemies[i].position, Vector3Add(move, knock));
        enemies[i].knockback = Vector3Scale(enemies[i].knockback, 0.85f);
        if (enemies[i].flash_timer > 0) enemies[i].flash_timer -= dt;
        
        // プレイヤーダメージ
        if (dist < 1.5f && player.dash_duration <= 0) {
            player.hp -= 1;
            if (player.hp <= 0) current_state = STATE_GAMEOVER;
        }

        // 弾当たり判定
        BoundingBox box = {
            (Vector3){enemies[i].position.x-1, 0, enemies[i].position.z-1},
            (Vector3){enemies[i].position.x+1, 2, enemies[i].position.z+1}
        };
        
        for (int b=0; b<MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            if (CheckCollisionBoxSphere(box, bullets[b].position, 0.3f)) {
                bullets[b].active = false;
                enemies[i].hp -= 10;
                enemies[i].flash_timer = 0.1f;
                Vector3 push = Vector3Normalize(bullets[b].velocity);
                enemies[i].knockback = Vector3Add(enemies[i].knockback, Vector3Scale(push, 20.0f));
                SpawnExplosion(bullets[b].position, YELLOW, 2);

                if (enemies[i].hp <= 0) {
                    enemies[i].active = false;
                    SpawnExplosion(enemies[i].position, enemies[i].type == ENEMY_TANK ? RED : ORANGE, 10);
                    player.exp += (enemies[i].type == ENEMY_TANK) ? 50 : 10;
                    if (player.exp >= player.next_level_exp) {
                        player.level++;
                        player.exp = 0;
                        player.next_level_exp *= 1.5;
                        if (player.level == 3) player.weapon_type = 1;
                    }
                }
            }
        }
    }

    // パーティクル
    for(int i=0; i<MAX_PARTICLES; i++){
        if(!particles[i].active) continue;
        particles[i].position = Vector3Add(particles[i].position, Vector3Scale(particles[i].velocity, dt));
        particles[i].life -= dt;
        if(particles[i].life <= 0) particles[i].active = false;
    }
}

void DrawGame() {
    ClearBackground((Color){10, 10, 15, 255});

    BeginMode3D(camera);
        rlPushMatrix();
        rlTranslatef(0, -0.1f, 0);
        DrawGrid(100, 2.0f); 
        rlPopMatrix();

        if (current_state != STATE_GAMEOVER) {
            Color pColor = (player.dash_duration > 0) ? SKYBLUE : BLUE;
            DrawMecha(player.position, player.facing_angle, pColor, player.walk_anim_timer, false);
            
            if (player.dash_cooldown > 0) {
                 Vector3 barPos = player.position; barPos.x -= 1.0f; barPos.z += 1.0f;
                 DrawCube(barPos, 2.0f * (player.dash_cooldown/2.0f), 0.2f, 0.2f, GRAY);
            }
        }

        for (int i=0; i<MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            
            // 落下中の敵には「影」を描画（重要：これが3Dゲームの定石）
            if (!enemies[i].is_grounded) {
                // 地面に影を描く
                DrawCylinder((Vector3){enemies[i].position.x, 0, enemies[i].position.z}, 
                             1.0f, 1.0f, 0.1f, 10, (Color){0,0,0,100});
            }

            Color eColor = (enemies[i].type == ENEMY_TANK) ? DARKPURPLE : RED;
            if (enemies[i].flash_timer > 0) eColor = WHITE;
            
            DrawMecha(enemies[i].position, 0, eColor, enemies[i].anim_timer, enemies[i].type == ENEMY_TANK);
            
            if (enemies[i].hp < enemies[i].max_hp) {
                Vector3 hpPos = enemies[i].position; hpPos.y += 3.0f;
                float ratio = enemies[i].hp / enemies[i].max_hp;
                DrawCube(hpPos, 2.0f * ratio, 0.2f, 0.2f, GREEN);
            }
        }

        for (int i=0; i<MAX_BULLETS; i++) {
            if (bullets[i].active) DrawSphere(bullets[i].position, 0.3f, YELLOW);
        }

        for (int i=0; i<MAX_PARTICLES; i++) {
            if (particles[i].active) DrawCube(particles[i].position, particles[i].size, particles[i].size, particles[i].size, particles[i].color);
        }
        
        // 狙っている場所のガイド
        Ray ray = GetMouseRay(GetMousePosition(), camera);
        float t = -ray.position.y / ray.direction.y;
        Vector3 aim = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        DrawCircle3D(aim, 0.6f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){0, 255, 0, 100});

    EndMode3D();

    DrawText(TextFormat("HP: %d", player.hp), 20, 20, 40, RED);
    DrawText(TextFormat("LV: %d", player.level), 20, 70, 40, GREEN);
    
    // 現在のモード表示
    if (difficulty == MODE_HARD) DrawText("MODE: HARD", SCREEN_WIDTH - 200, 20, 30, RED);
    else DrawText("MODE: NORMAL", SCREEN_WIDTH - 200, 20, 30, SKYBLUE);

    if (player.dash_cooldown <= 0) DrawText("DASH READY [SPACE]", 20, 120, 20, SKYBLUE);

    if (current_state == STATE_GAMEOVER) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0,0,0,200});
        DrawText("GAME OVER", SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 50, 80, RED);
        DrawText("PRESS 'R' TO RETURN TITLE", SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 + 50, 30, WHITE);
    }
}

void SpawnBullet(Vector3 direction) {
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].position = (Vector3){player.position.x, 1.5f, player.position.z};
            bullets[i].velocity = Vector3Scale(direction, 30.0f);
            bullets[i].life_time = 1.0f;
            break;
        }
    }
}

void SpawnEnemy() {
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].active = true;
            
            // スポーン位置決定
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float dist = 30.0f;
            
            if (difficulty == MODE_HARD && GetRandomValue(0, 100) < 40) {
                // 40%の確率で空から降ってくる（ハード限定）
                // プレイヤーの近くに落ちる
                enemies[i].position = (Vector3){
                    player.position.x + (float)GetRandomValue(-15, 15),
                    20.0f, // 空中
                    player.position.z + (float)GetRandomValue(-15, 15)
                };
                enemies[i].is_grounded = false;
                enemies[i].vertical_speed = 0.0f;
            } else {
                // 通常スポーン（地上）
                enemies[i].position = (Vector3){
                    player.position.x + cosf(angle) * dist,
                    0,
                    player.position.z + sinf(angle) * dist
                };
                enemies[i].is_grounded = true;
            }

            if (GetRandomValue(0, 100) < 20) {
                enemies[i].type = ENEMY_TANK;
                enemies[i].speed = 3.0f;
                enemies[i].max_hp = 100;
                enemies[i].hp = 100;
            } else {
                enemies[i].type = ENEMY_DRONE;
                enemies[i].speed = 7.0f;
                enemies[i].max_hp = 20;
                enemies[i].hp = 20;
            }
            enemies[i].knockback = (Vector3){0,0,0};
            enemies[i].flash_timer = 0;
            enemies[i].anim_timer = 0;
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