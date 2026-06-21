#pragma once

#include "engine/Systems.hpp"

#include <memory>

namespace engine {

/// Controller가 possess할 수 있는 Actor의 최소 기반이다.
class Pawn : public Actor {
public:
    Pawn(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;

    virtual void addMovementInput(const glm::vec3& direction, float scale);
};

class Character : public Pawn {
public:
    /// 큐브 표시와 Box 충돌을 조합하고 입력을 cm/s 이동으로 바꾼다.
    Character(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void onConstruction() override;
    void tick(float deltaTime) override;
    void addMovementInput(const glm::vec3& direction, float scale) override;

    [[nodiscard]] float moveSpeed() const;
    void setMoveSpeed(float speed);
    [[nodiscard]] const MovementResult& lastMovement() const;

private:
    BoxComponent* collision_{nullptr};
    glm::vec3 pendingMovement_{0.0F};
    MovementResult lastMovement_;
    float moveSpeed_{400.0F};
    float stepHeight_{35.0F};
};

class Controller : public Actor {
public:
    /// Pawn을 소유하지 않고 제어 대상 포인터와 저장용 GUID만 기억한다.
    Controller(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void possess(Pawn* pawn);
    [[nodiscard]] Pawn* pawn() const;
    [[nodiscard]] Guid pawnGuid() const;
    void setPawnGuid(Guid guid);
    void onActorDestroyed(Actor& actor) override;

private:
    Pawn* pawn_{nullptr};
};

class PlayerController : public Controller {
public:
    /// InputSystem의 MoveForward/MoveRight 축을 possess한 Pawn에 전달한다.
    PlayerController(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void tick(float deltaTime) override;
};

class PlayerStart : public Actor {
public:
    PlayerStart(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;
    void onConstruction() override;
};

class GameMode : public Actor {
public:
    GameMode(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;
};

class GameInstance : public Object {
public:
    /// World가 바뀌어도 EngineRuntime과 함께 유지되는 게임 세션 객체다.
    GameInstance();
    [[nodiscard]] std::string_view typeName() const override;
};

enum class EditorMode {
    Edit,
    Simulate,
    PlayInEditor,
};

class EngineRuntime {
public:
    /// Edit World와 임시 PIE World의 소유권, 실행 모드, 공용 시스템을 관리한다.
    EngineRuntime();

    void setEditWorld(std::unique_ptr<World> world);
    [[nodiscard]] World& editWorld();
    [[nodiscard]] const World& editWorld() const;
    [[nodiscard]] World& activeWorld();
    [[nodiscard]] const World& activeWorld() const;

    bool startSimulate();
    bool startPlayInEditor();
    void stop();
    void tick(float deltaTime);

    [[nodiscard]] EditorMode mode() const;
    [[nodiscard]] InputSystem& input();
    [[nodiscard]] OutputLog& log();
    [[nodiscard]] AssetRegistry& assets();
    [[nodiscard]] TransactionStack& transactions();

private:
    void connectInput(World& world);

    GameInstance gameInstance_;
    std::unique_ptr<World> editWorld_;
    std::unique_ptr<World> playWorld_;
    InputSystem input_;
    OutputLog log_;
    AssetRegistry assets_;
    TransactionStack transactions_;
    EditorMode mode_{EditorMode::Edit};
};

} // namespace engine
