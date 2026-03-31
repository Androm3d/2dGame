#ifndef _GAME_CONSTANTS_INCLUDE
#define _GAME_CONSTANTS_INCLUDE


// =============================================================================
//  Constantes compartidas por todas las entidades
// =============================================================================

#define GRAVITY                  1400.0f  // px/s^2 aceleracion de gravedad
#define FALL_STEP                   4     // px por tick de caida (fisicas enteras)
#define JUMP_ANGLE_STEP             4     // grados por tick en la simulacion de salto del BFS
#define PATH_RECALC_FRAMES         30     // ticks entre recalculos del pathfinder BFS
#define HIT_INVINCIBILITY_FRAMES  150     // ticks de invencibilidad tras recibir danio
#define HIT_BLINK_FRAMES           50     // ticks que el sprite parpadea (subconjunto de invencibilidad)
#define KNOCKBACK_FRAMES            8     // ticks de retroceso tras un golpe
#define KNOCKBACK_SPEED             5     // px por tick durante el retroceso


// =============================================================================
//  Player — Samurai
// =============================================================================

// Samurai.png: 512x512, frame 56x80
#define PLAYER_FRAME_WIDTH           56
#define PLAYER_FRAME_HEIGHT          80
#define PLAYER_RENDER_WIDTH          45   // quad en pantalla (escalado a 64px de alto)
#define PLAYER_RENDER_HEIGHT         64

// Samurai_Attack.png: 512x128, frame 88x106
#define PLAYER_ATTACK_FRAME_WIDTH    88
#define PLAYER_ATTACK_FRAME_HEIGHT  106
#define PLAYER_ATTACK_RENDER_WIDTH   80   // mas ancho para mostrar el swing de la espada
#define PLAYER_ATTACK_RENDER_HEIGHT  96

// Frames por animacion (filas del spritesheet)
#define PLAYER_IDLE_FRAMES            6
#define PLAYER_RUN_FRAMES             8
#define PLAYER_JUMP_UP_FRAMES         5
#define PLAYER_JUMP_FALL_FRAMES       4
#define PLAYER_HURT_FRAMES            3
#define PLAYER_DEAD_FRAMES            6
#define PLAYER_PROTECT_FRAMES         2
#define PLAYER_ATTACK_FRAMES          5

// Offsets Y de cada fila en Samurai.png (px)
#define PLAYER_ROW_IDLE               0
#define PLAYER_ROW_RUN               80
#define PLAYER_ROW_JUMP             160
#define PLAYER_ROW_HURT             240
#define PLAYER_ROW_DEAD             320
#define PLAYER_ROW_PROTECT          400

// Movimiento
#define PLAYER_JUMP_HEIGHT           96   // altura maxima de salto (px)
#define PLAYER_WALK_SPEED        180.0f   // px/s horizontal
#define PLAYER_CLIMB_SPEED       120.0f   // px/s en escaleras
#define PLAYER_DROP_THROUGH_MS      180   // ms atravesando plataformas one-way
#define PLAYER_DROP_THROUGH_NUDGE  4.0f   // px de empuje para atravesar la plataforma
#define PLAYER_SPRING_MULTIPLIER      3   // el muelle lanza sqrt(N)x mas alto
#define PLAYER_DASH_DURATION_MS    1000   // duracion del dash (ms)
#define PLAYER_DASH_DISTANCE_BASE 60.0f   // distancia base del dash (px)
#define PLAYER_SPRING_COOLDOWN_MS  120    // ms de cooldown tras rebotar en un muelle

// Combate (en game frames, ~16 ms cada uno a 60 fps)
#define PLAYER_PARRY_FRAMES          22   // ventana activa del parry
#define PLAYER_PARRY_COOLDOWN        55   // cooldown tras un parry
#define PLAYER_ATTACK_COOLDOWN       55   // cooldown tras un ataque


// =============================================================================
//  Enemy1 — Arquero
// =============================================================================

// Enemy1.png: 256x192, frame 64x48
#define ENEMY_TEX_FRAME_WIDTH        64   // ancho UV en la textura
#define ENEMY_TEX_FRAME_HEIGHT       48   // alto UV en la textura
#define ENEMY_FRAME_WIDTH            77   // ancho del quad en pantalla (escalado a 64px alto)
#define ENEMY_FRAME_HEIGHT           64   // alto del quad en pantalla
#define ENEMY_HITBOX_WIDTH           32   // hitbox centrada en el quad
#define ENEMY_HITBOX_HEIGHT          32
#define ENEMY_SPEED                   1   // px por tick horizontal
#define ENEMY_JUMP_HEIGHT           112   // altura maxima de salto (px)
#define ENEMY_RUN_FRAMES              8
#define ENEMY_JUMP_UP_FRAMES          4
#define ENEMY_JUMP_FALL_FRAMES        4
#define ENEMY_JUMP_FALL_START         4   // indice del primer frame de caida en la fila de salto
#define ENEMY_HURT_FRAMES             3
#define ENEMY_DEATH_FRAMES            5

// Offsets Y de cada fila en Enemy1.png (px)
#define E1_ROW_RUN                    0
#define E1_ROW_JUMP                  48
#define E1_ROW_HURT                  96
#define E1_ROW_DEATH                144

// Enemy1_Shot.png: 768x232, frame 96x116, 13 frames (8 fila 0 + 5 fila 1)
#define SHOT_FRAME_WIDTH             96
#define SHOT_FRAME_HEIGHT           116
#define SHOT_RENDER_HEIGHT           64
#define SHOT_FRAMES_ROW0              8
#define SHOT_FRAMES_ROW1              5
#define SHOT_DETECT_RANGE           200   // px de rango de deteccion horizontal
#define SHOT_DETECT_VERTICAL         48   // px de tolerancia vertical
#define SHOT_COOLDOWN_FRAMES         90   // ticks entre disparos

// Flecha (proyectil)
#define ARROW_SPEED                   4   // px por tick
#define ARROW_SIZE                   64   // tamanio del sprite (cuadrado)
#define ARROW_HITBOX_W               32   // hitbox mas estrecha que el sprite
#define ARROW_HITBOX_H               12   // fina verticalmente, solo impactos directos
#define ENEMY_DASH_DURATION_MS     1000   // duracion del dash (ms)
#define ENEMY_DASH_DISTANCE_BASE  60.0f   // distancia base del dash (px)


// =============================================================================
//  Enemy2 — Espadachin
// =============================================================================

// Enemy2.png: 768x560, frame 96x112
#define E2_FRAME_WIDTH               96
#define E2_FRAME_HEIGHT             112
#define E2_RENDER_WIDTH              55   // quad en pantalla (escalado a 64px alto)
#define E2_RENDER_HEIGHT             64
#define E2_HITBOX_WIDTH              32
#define E2_HITBOX_HEIGHT             32
#define E2_SPEED                      1   // px por tick horizontal
#define E2_JUMP_HEIGHT              112   // altura maxima de salto (px)
#define E2_RUN_FRAMES                 8
#define E2_JUMP_UP_FRAMES             4
#define E2_JUMP_FALL_FRAMES           3
#define E2_ATTACK_FRAMES              4
#define E2_HURT_FRAMES                2
#define E2_DEATH_FRAMES               6

// Offsets Y de cada fila en Enemy2.png (px)
#define E2_ROW_RUN                    0
#define E2_ROW_JUMP                 112
#define E2_ROW_ATTACK               224
#define E2_ROW_HURT                 336
#define E2_ROW_DEATH                448

// Deteccion melee (rango de espada)
#define E2_MELEE_DETECT_RANGE        36   // px horizontal — aprox. 1 tile
#define E2_MELEE_DETECT_VERT         48   // px de tolerancia vertical
#define E2_MELEE_HITBOX_REACH        48   // px que la hitbox de espada se extiende al frente
#define E2_ATTACK_COOLDOWN           90   // ticks entre ataques melee
#define E2_DASH_DURATION_MS        1000   // duracion del dash (ms)
#define E2_DASH_DISTANCE_BASE     60.0f   // distancia base del dash (px)


// =============================================================================
//  Enemy3 — Mago
// =============================================================================

// Enemy3.png: 960x576, frame 96x96
#define E3_FRAME_WIDTH               96
#define E3_FRAME_HEIGHT              96
#define E3_RENDER_WIDTH              64   // quad en pantalla (96 * 64/96 = 64)
#define E3_RENDER_HEIGHT             64
#define E3_HITBOX_WIDTH              32
#define E3_HITBOX_HEIGHT             32
#define E3_SPEED                      1   // px por tick horizontal
#define E3_JUMP_HEIGHT              112   // altura maxima de salto (px)
#define E3_RUN_FRAMES                 8
#define E3_JUMP_UP_FRAMES             5
#define E3_JUMP_FALL_FRAMES           5
#define E3_HURT_FRAMES                2
#define E3_DEAD_FRAMES               10
#define E3_ATTACK_FRAMES              7   // fire cast (fila 5)
#define E3_ATTACK2_FRAMES            10   // melee (fila 4)

// Offsets Y de cada fila en Enemy3.png (px)
#define E3_ROW_RUN                    0
#define E3_ROW_JUMP                  96
#define E3_ROW_HURT                 192
#define E3_ROW_DEAD                 288
#define E3_ROW_ATK2                 384   // melee — fila 4
#define E3_ROW_ATTACK               480   // fire cast — fila 5

// Enemy3_Fire.png: 352x48, frame 32x48, 11 frames
#define FIRE_FRAME_WIDTH             32
#define FIRE_FRAME_HEIGHT            48
#define FIRE_RENDER_WIDTH            32
#define FIRE_RENDER_HEIGHT           48
#define FIRE_FRAMES                  11
#define FIRE_ANIM_FPS                 5   // 11 frames @ 5fps = ~2.2s
#define FIRE_SPEED                    3   // px por game frame
#define FIRE_DETECT_RANGE           180   // px de rango de deteccion horizontal
#define FIRE_DETECT_VERT             48   // px de tolerancia vertical

// Deteccion melee (rango corto)
#define E3_MELEE_DETECT_RANGE        40   // px horizontal
#define E3_MELEE_DETECT_VERT         48   // px de tolerancia vertical
#define E3_ATTACK_COOLDOWN           90   // ticks entre ataques (fuego o melee)
#define E3_DASH_DURATION_MS        1000   // duracion del dash (ms)
#define E3_DASH_DISTANCE_BASE     60.0f   // distancia base del dash (px)


#endif // _GAME_CONSTANTS_INCLUDE
