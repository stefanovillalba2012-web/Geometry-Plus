// Geometry Plus+  —  por Bosked
// Idea: cada portal nuevo es un "clon" de un portal vanilla (para que el juego
// haga el cambio de modo/velocidad/tamaño), con un ID propio (4001+) y un
// ajuste extra que aplicamos justo después de que el jugador lo toca.
//
// OJO: no pude compilarlo ni probarlo aquí. Los nombres de campos/offsets
// pueden necesitar ajustes según tu versión de Geode y GD (2.2074).

#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------- definiciones

enum class Kind { Vehicle, Speed, Size, Camera, Original };

struct PlusPortal {
    int id;          // ID propio del objeto
    int baseId;      // portal vanilla que clonamos
    Kind kind;
    float value;     // gravedad / velocidad / escala (según el tipo)
    ccColor3B color; // tinte para distinguirlo en el editor
    bool platformerOnly;
};

// IDs vanilla de portales: cubo 12, nave 13, bola 47, ufo 111, wave 660,
// robot 745, araña 1331, swing 1933, tamaño normal 99, mini 101, velocidad x1 201
static const std::vector<PlusPortal> PORTALS = {
    // ---- 10 portales de vehículo (variantes de física de modos existentes)
    {4001, 13,   Kind::Vehicle, 0.70f, {255, 120, 120}, false}, // Nave pesada
    {4002, 13,   Kind::Vehicle, 1.30f, {120, 200, 255}, false}, // Nave ligera
    {4003, 47,   Kind::Vehicle, 0.60f, {255, 160,  60}, false}, // Bola lenta
    {4004, 47,   Kind::Vehicle, 1.40f, {255, 255,  80}, false}, // Bola turbo
    {4005, 111,  Kind::Vehicle, 0.75f, {180, 120, 255}, false}, // UFO flotante
    {4006, 111,  Kind::Vehicle, 1.25f, {255, 100, 220}, false}, // UFO nervioso
    {4007, 660,  Kind::Vehicle, 0.80f, {100, 255, 160}, false}, // Wave suave
    {4008, 660,  Kind::Vehicle, 1.20f, {  0, 255, 255}, false}, // Wave afilada
    {4009, 1933, Kind::Vehicle, 0.80f, {255, 200, 200}, false}, // Swing pesado
    {4010, 1933, Kind::Vehicle, 1.25f, {200, 255, 200}, false}, // Swing ágil

    // ---- 8 velocidades nuevas (clonan el portal x1, id 201)
    {4101, 201, Kind::Speed, 0.40f, {200, 200, 200}, false},
    {4102, 201, Kind::Speed, 0.55f, {170, 220, 255}, false},
    {4103, 201, Kind::Speed, 0.85f, {120, 255, 120}, false},
    {4104, 201, Kind::Speed, 1.20f, {255, 255, 120}, false},
    {4105, 201, Kind::Speed, 1.50f, {255, 190,  90}, false},
    {4106, 201, Kind::Speed, 2.50f, {255, 120,  90}, false},
    {4107, 201, Kind::Speed, 3.50f, {255,  80,  80}, false},
    {4108, 201, Kind::Speed, 4.50f, {255,   0, 120}, false},

    // ---- 3 portales de tamaño (clonan el portal normal, id 99)
    {4201, 99, Kind::Size, 0.45f, {255, 255, 255}, false}, // Micro
    {4202, 99, Kind::Size, 1.40f, {255, 255, 255}, false}, // Grande
    {4203, 99, Kind::Size, 1.90f, {255, 255, 255}, false}, // Gigante

    // ---- cámara libre (value 1 = activar, 0 = desactivar)
    {4301, 99, Kind::Camera, 1.f, {  0, 255, 200}, false},
    {4302, 99, Kind::Camera, 0.f, {255,  60,  60}, false},

    // ---- 3 vehículos de modo plataforma (solo actúan en niveles platformer)
    {4401, 745,  Kind::Vehicle, 0.85f, {255, 140,  0}, true},  // Robot pesado
    {4402, 1331, Kind::Vehicle, 1.25f, {160,   0, 255}, true}, // Araña veloz
    {4403, 13,   Kind::Vehicle, 0.90f, {  0, 180, 255}, true}, // Nave de plataforma

    // ---- 10 vehículos ORIGINALES (física propia, ver PlusPlayer::runVehicle)
    // Clonan el portal de cubo (12) solo como "chasis"; el movimiento lo
    // controla el mod desde cero.
    {4501, 12, Kind::Original, 0, {255,  90,  40}, false}, // Cohete
    {4502, 12, Kind::Original, 1, { 90, 255, 200}, false}, // Péndulo
    {4503, 12, Kind::Original, 2, {255, 230,  60}, false}, // Resorte
    {4504, 12, Kind::Original, 3, {150, 200, 255}, false}, // Planeador
    {4505, 12, Kind::Original, 4, {220,  60, 255}, false}, // Imán
    {4506, 12, Kind::Original, 5, {255,  60, 120}, false}, // Dasher
    {4507, 12, Kind::Original, 6, {255, 170, 220}, false}, // Globo
    {4508, 12, Kind::Original, 7, {120, 255,  90}, false}, // Rebotador
    {4509, 12, Kind::Original, 8, { 40, 220, 120}, false}, // Serpiente
    {4510, 12, Kind::Original, 9, {255, 140,  40}, false}, // Turbina
};

static const PlusPortal* findPortal(int id) {
    for (auto const& p : PORTALS) if (p.id == id) return &p;
    return nullptr;
}

static bool s_freeCam = false;

// ------------------------------------------------- crear el objeto (editor/nivel)

class $modify(PlusGameObject, GameObject) {
    static GameObject* createWithKey(int key) {
        auto* p = findPortal(key);
        if (!p) return GameObject::createWithKey(key);

        auto* obj = GameObject::createWithKey(p->baseId);
        if (!obj) return nullptr;
        obj->m_objectID = key;                 // se guarda con nuestro ID
        obj->setColor(p->color);               // tinte distintivo
        return obj;
    }
};

// ------------------------------------------------------------ efecto al tocarlo

// Índices de los vehículos originales
enum Veh { V_ROCKET, V_PENDULUM, V_SPRING, V_GLIDER, V_MAGNET,
           V_DASHER, V_BALLOON, V_BOUNCER, V_SERPENT, V_TURBINE };

static bool isVanillaModePortal(int id) {
    switch (id) {
        case 12: case 13: case 47: case 111:
        case 660: case 745: case 1331: case 1933: return true;
        default: return false;
    }
}

class $modify(PlusPlayer, PlayerObject) {
    struct Fields {
        int   veh = -1;      // vehículo original activo (-1 = ninguno)
        bool  hold = false;
        float t = 0.f, charge = 0.f, vel = 0.f, acc = 0.f;
        float thrust = 0.f, boost = 0.f;
        int   dashes = 2;
    };

    void stopVehicle() {
        m_fields->veh = -1;
        m_fields->hold = false;
        m_fields->charge = m_fields->vel = m_fields->acc = 0.f;
        m_fields->thrust = m_fields->boost = m_fields->t = 0.f;
        this->setScaleY(this->getScaleX());
    }

    void startVehicle(int v) {
        if (m_fields->veh == v) return; // seguimos dentro del mismo portal
        stopVehicle();
        m_fields->veh = v;
        m_fields->dashes = 2;
    }

    // Se llama cada frame ANTES de la física normal. Trabajamos en "u":
    // velocidad vertical positiva = hacia donde apunta la gravedad contraria.
    void runVehicle(float dt) {
        auto* f = m_fields.self();
        float k  = dt * 60.f;
        float up = m_isUpsideDown ? -1.f : 1.f;
        float u  = static_cast<float>(m_yVelocity) * up;
        f->t += dt;

        switch (f->veh) {
        case V_ROCKET: // aceleración que va creciendo: tiene inercia
            f->acc += (f->hold ? 1.f : -1.f) * 0.05f * k;
            f->acc = std::clamp(f->acc, -0.6f, 0.6f);
            f->vel = std::clamp(f->vel + f->acc * k, -10.f, 10.f);
            if (m_isOnGround && f->vel < 0.f) { f->vel = 0.f; f->acc = 0.f; }
            u = f->vel;
            break;
        case V_PENDULUM: // oscila solo; mantener = sube más
            u = std::sin(f->t * 5.f) * 7.f + (f->hold ? 2.5f : -1.f);
            break;
        case V_SPRING: // mantener = se comprime (carga); soltar = salta
            if (f->hold && m_isOnGround) {
                f->charge = std::min(f->charge + 0.05f * k, 1.f);
                this->setScaleY(this->getScaleX() * (1.f - 0.35f * f->charge));
            }
            break;
        case V_GLIDER: // tocar = aleteo; mantener = planea (cae lento)
            if (f->hold) u = std::max(u, -1.8f);
            break;
        case V_MAGNET: // la fuerza crece mientras más tiempo mantienes
            f->charge = f->hold ? std::min(f->charge + 0.03f * k, 1.f)
                                : std::max(f->charge - 0.06f * k, 0.f);
            u += (f->hold ? 1.f : -1.f) * (0.4f + f->charge * 1.4f) * k;
            u = std::clamp(u, -12.f, 12.f);
            break;
        case V_DASHER: // 2 dashes en el aire, se recargan al tocar suelo
            if (m_isOnGround) f->dashes = 2;
            break;
        case V_BALLOON: // flota solo; mantener = se hunde
            u += (f->hold ? -0.55f : 0.35f) * k;
            u = std::clamp(u, -6.f, 4.5f);
            break;
        case V_BOUNCER: // rebota solo; mantener = "pisotón" que carga el rebote
            if (f->hold) { u = std::min(u, -10.f); f->boost = std::min(f->boost + 0.15f * k, 6.f); }
            if (m_isOnGround && u <= 0.f) { u = 9.f + f->boost; f->boost = 0.f; }
            break;
        case V_SERPENT: // sinusoide; mantener = más amplitud
            u = std::cos(f->t * 6.f) * (f->hold ? 8.f : 3.f);
            break;
        case V_TURBINE: // mantener = carga (cae lento); soltar = empuje largo
            if (f->hold) {
                f->charge = std::min(f->charge + 0.02f * k, 1.f);
                u = std::max(u, -3.f);
            } else if (f->thrust > 0.02f) {
                u = std::min(u + f->thrust * k, 14.f);
                f->thrust *= std::pow(0.92f, k);
            }
            break;
        }
        m_yVelocity = u * up;
    }

    void update(float dt) {
        if (m_fields->veh >= 0) this->runVehicle(dt);
        PlayerObject::update(dt);
    }

    bool pushButton(PlayerButton btn) {
        if (m_fields->veh < 0 || btn != PlayerButton::Jump)
            return PlayerObject::pushButton(btn);

        auto* f = m_fields.self();
        float up = m_isUpsideDown ? -1.f : 1.f;
        f->hold = true;
        if (f->veh == V_GLIDER) {
            m_yVelocity = std::max(static_cast<float>(m_yVelocity) * up, 6.5f) * up;
        } else if (f->veh == V_DASHER && f->dashes > 0) {
            f->dashes--;
            m_yVelocity = 12.f * up;
        }
        return true; // bloquea el salto normal del cubo
    }

    bool releaseButton(PlayerButton btn) {
        if (m_fields->veh < 0 || btn != PlayerButton::Jump)
            return PlayerObject::releaseButton(btn);

        auto* f = m_fields.self();
        float up = m_isUpsideDown ? -1.f : 1.f;
        f->hold = false;
        if (f->veh == V_SPRING) {
            if (m_isOnGround && f->charge > 0.05f) {
                m_yVelocity = (9.f + f->charge * 10.f) * up;
                m_isOnGround = false;
            }
            f->charge = 0.f;
            this->setScaleY(this->getScaleX());
        } else if (f->veh == V_TURBINE) {
            f->thrust = f->charge * 1.4f;
            f->charge = 0.f;
        }
        return true;
    }

    void resetObject() {
        PlayerObject::resetObject();
        stopVehicle();
        s_freeCam = false;
    }

    // NOTA: en algunas versiones de GD esta función devuelve void; ajusta la
    // firma si el compilador te lo pide.
    void collidedWithObject(float dt, GameObject* obj, cocos2d::CCRect rect, bool anyFree) {
        int id = obj->m_objectID;
        auto* p = findPortal(id);

        if (!p) { // objeto normal: si es un portal de modo vanilla, salimos del vehículo original
            PlayerObject::collidedWithObject(dt, obj, rect, anyFree);
            if (isVanillaModePortal(id)) stopVehicle();
            return;
        }
        if (p->platformerOnly && !m_isPlatformer) return;
        if (p->kind == Kind::Original && m_isPlatformer) return;
        if (p->kind == Kind::Camera) { s_freeCam = p->value > 0.5f; return; }

        // engañamos al juego: se ve como el portal vanilla durante la colisión
        obj->m_objectID = p->baseId;
        PlayerObject::collidedWithObject(dt, obj, rect, anyFree);
        obj->m_objectID = id;

        switch (p->kind) {
            case Kind::Vehicle:
                stopVehicle();
                m_gravityMod = p->value;
                break;
            case Kind::Original:
                startVehicle(static_cast<int>(p->value));
                break;
            case Kind::Speed:
                m_playerSpeed = p->value;
                break;
            case Kind::Size:
                m_vehicleSize = p->value;
                this->setScale(p->value);
                break;
            default: break;
        }
    }
};

// ----------------------------------------------------------------- cámara libre

class $modify(PlusLayer, GJBaseGameLayer) {
    void updateCamera(float dt) {
        if (!s_freeCam) return GJBaseGameLayer::updateCamera(dt);

        // Cámara libre: avanza en X con el jugador pero la Y se suaviza
        // para no seguir cada salto.
        float y = m_objectLayer->getPositionY();
        GJBaseGameLayer::updateCamera(dt);
        float target = m_objectLayer->getPositionY();
        m_objectLayer->setPositionY(y + (target - y) * 0.05f);
    }
};

// ------------------------------------------------------------------ editor

class $modify(PlusEditor, EditorUI) {
    $override
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        // Busca la barra que contiene portales (el botón del portal de nave, id 13)
        // y agrega ahí nuestros botones.
        for (auto* bar : CCArrayExt<EditButtonBar*>(m_createButtonBars)) {
            bool hasPortals = false;
            for (auto* item : CCArrayExt<CCNode*>(bar->m_buttonArray)) {
                if (item->getTag() == 13) { hasPortals = true; break; }
            }
            if (!hasPortals) continue;

            for (auto const& p : PORTALS) {
                bar->m_buttonArray->addObject(this->getCreateBtn(p.id, 4));
            }
            bar->reloadItems(
                GameManager::get()->getIntGameVariable("0049"), // filas
                GameManager::get()->getIntGameVariable("0050")  // columnas
            );
            break;
        }
        return true;
    }
};
