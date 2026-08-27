#pragma once
#include <cstdint>

// ==================================================
// 🔧 OFFSETS — FREE FIRE 2.130.1
// ✅ SÓ ALTERE AQUI QUANDO O JOGO ATUALIZAR!
// ==================================================

// === PONTEIROS RAIZ ===
#define INIT_BASE                0xAAA5B20
#define CURRENT_MATCH            0x50
#define LOCAL_PLAYER             0x94
#define ENTITY_DICTIONARY        0x68

// === JOGADOR ===
#define PLAYER_IS_DEAD           0x50
#define PLAYER_IS_TEAMMATE       0x59

// === AVATAR / VISIBILIDADE ===
#define AVATAR_INSTANCE          0xA8
#define AVATAR_IS_VISIBLE        0x95

// === CÂMERA / MATRIZ ===
#define CAMERA_TRANSFORM         0x250
#define MATRIX_WORLD_TO_SCREEN   0xE8

// === BONES (ESQUELETO) ===
#define BONE_HEAD                0x460
#define BONE_HIP                 0x464
#define BONE_RIGHT_FOOT          0x488

// === DICTIONARY IL2CPP ===
#define DICT_PAIRS_OFFSET        0x40
#define DICT_COUNT_OFFSET        0x48
#define DICT_ENTITY_OFFSET       0x08

// === BONE STRUCT ===
#define TRANSFORM_NODE_POS       0x28
