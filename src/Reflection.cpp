#include "engine/Gameplay.hpp"

#include <algorithm>
#include <utility>

namespace {

using namespace engine;

constexpr PropertyFlags kEditSave =
    PropertyFlags::Editable | PropertyFlags::Serializable;

template<typename ObjectType, typename ValueType>
PropertyDescriptor property(
    std::string name,
    std::string category,
    PropertyType type,
    ValueType (ObjectType::*getter)() const,
    void (ObjectType::*setter)(ValueType)
) {
    return {
        std::move(name),
        std::move(category),
        type,
        kEditSave,
        [getter](const Object& object) -> PropertyValue {
            return (dynamic_cast<const ObjectType&>(object).*getter)();
        },
        [setter](Object& object, const PropertyValue& value) {
            const auto* typedValue = std::get_if<ValueType>(&value);
            auto* typedObject = dynamic_cast<ObjectType*>(&object);
            if (typedValue == nullptr || typedObject == nullptr) {
                return false;
            }
            (typedObject->*setter)(*typedValue);
            return true;
        },
    };
}

PropertyDescriptor nameProperty() {
    return {
        "Name",
        "Object",
        PropertyType::String,
        kEditSave,
        [](const Object& object) -> PropertyValue { return object.name(); },
        [](Object& object, const PropertyValue& value) {
            const auto* name = std::get_if<std::string>(&value);
            if (name == nullptr) {
                return false;
            }
            object.setName(*name);
            return true;
        },
    };
}

PropertyDescriptor tickEnabledProperty() {
    return {
        "TickEnabled",
        "Tick",
        PropertyType::Boolean,
        kEditSave,
        [](const Object& object) -> PropertyValue {
            if (const auto* actor = dynamic_cast<const Actor*>(&object)) {
                return actor->tickSettings().enabled;
            }
            return dynamic_cast<const ActorComponent&>(object)
                .tickSettings()
                .enabled;
        },
        [](Object& object, const PropertyValue& value) {
            const auto* enabled = std::get_if<bool>(&value);
            if (enabled == nullptr) {
                return false;
            }
            if (auto* actor = dynamic_cast<Actor*>(&object)) {
                actor->tickSettings().enabled = *enabled;
                return true;
            }
            if (auto* component = dynamic_cast<ActorComponent*>(&object)) {
                component->tickSettings().enabled = *enabled;
                return true;
            }
            return false;
        },
    };
}

PropertyDescriptor tickIntervalProperty() {
    return {
        "TickInterval",
        "Tick",
        PropertyType::Float,
        kEditSave,
        [](const Object& object) -> PropertyValue {
            if (const auto* actor = dynamic_cast<const Actor*>(&object)) {
                return actor->tickSettings().interval;
            }
            return dynamic_cast<const ActorComponent&>(object)
                .tickSettings()
                .interval;
        },
        [](Object& object, const PropertyValue& value) {
            const auto* interval = std::get_if<float>(&value);
            if (interval == nullptr) {
                return false;
            }
            if (auto* actor = dynamic_cast<Actor*>(&object)) {
                actor->tickSettings().interval = std::max(*interval, 0.0F);
                return true;
            }
            if (auto* component = dynamic_cast<ActorComponent*>(&object)) {
                component->tickSettings().interval =
                    std::max(*interval, 0.0F);
                return true;
            }
            return false;
        },
    };
}

PropertyDescriptor tickGroupProperty() {
    return {
        "TickGroup",
        "Tick",
        PropertyType::Integer,
        kEditSave,
        [](const Object& object) -> PropertyValue {
            if (const auto* actor = dynamic_cast<const Actor*>(&object)) {
                return static_cast<int>(actor->tickSettings().group);
            }
            return static_cast<int>(
                dynamic_cast<const ActorComponent&>(object)
                    .tickSettings()
                    .group
            );
        },
        [](Object& object, const PropertyValue& value) {
            const auto* group = std::get_if<int>(&value);
            if (group == nullptr || *group < 0 ||
                *group > static_cast<int>(TickGroup::PostUpdate)) {
                return false;
            }
            if (auto* actor = dynamic_cast<Actor*>(&object)) {
                actor->tickSettings().group = static_cast<TickGroup>(*group);
                return true;
            }
            if (auto* component = dynamic_cast<ActorComponent*>(&object)) {
                component->tickSettings().group =
                    static_cast<TickGroup>(*group);
                return true;
            }
            return false;
        },
    };
}

PropertyDescriptor relativeVectorProperty(
    std::string name,
    glm::vec3 Transform::*member,
    void (SceneComponent::*setter)(const glm::vec3&)
) {
    return {
        std::move(name),
        "Transform",
        PropertyType::Vector3,
        kEditSave,
        [member](const Object& object) -> PropertyValue {
            return dynamic_cast<const SceneComponent&>(object)
                .relativeTransform().*member;
        },
        [setter](Object& object, const PropertyValue& value) {
            const auto* vector = std::get_if<glm::vec3>(&value);
            auto* component = dynamic_cast<SceneComponent*>(&object);
            if (vector == nullptr || component == nullptr) {
                return false;
            }
            (component->*setter)(*vector);
            return true;
        },
    };
}

} // namespace

namespace engine {

void registerEngineTypes() {
    ReflectionRegistry& registry = ReflectionRegistry::instance();
    if (registry.find("Object") != nullptr) {
        return;
    }

    const TypeDescriptor& object = registry.registerType({
        "Object",
        nullptr,
        [](Object* outer, std::string name) {
            return std::make_unique<Object>(std::move(name), outer);
        },
        {nameProperty()},
    });
    const TypeDescriptor& world = registry.registerType({
        "World",
        &object,
        [](Object*, std::string name) {
            return std::make_unique<World>(std::move(name));
        },
        {},
    });
    const TypeDescriptor& actor = registry.registerType({
        "Actor",
        &object,
        [](Object* outer, std::string name) {
            return std::make_unique<Actor>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {tickEnabledProperty(), tickIntervalProperty(), tickGroupProperty()},
    });
    const TypeDescriptor& actorComponent = registry.registerType({
        "ActorComponent",
        &object,
        [](Object* outer, std::string name) {
            return std::make_unique<ActorComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {tickEnabledProperty(), tickIntervalProperty(), tickGroupProperty()},
    });
    const TypeDescriptor& sceneComponent = registry.registerType({
        "SceneComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<SceneComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            relativeVectorProperty(
                "Location",
                &Transform::location,
                &SceneComponent::setRelativeLocation
            ),
            relativeVectorProperty(
                "Rotation",
                &Transform::rotationDegrees,
                &SceneComponent::setRelativeRotation
            ),
            relativeVectorProperty(
                "Scale",
                &Transform::scale,
                &SceneComponent::setRelativeScale
            ),
        },
    });
    const TypeDescriptor& primitive = registry.registerType({
        "PrimitiveComponent",
        &sceneComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<PrimitiveComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<PrimitiveComponent, bool>(
                "Visible",
                "Rendering",
                PropertyType::Boolean,
                &PrimitiveComponent::visible,
                &PrimitiveComponent::setVisible
            ),
            {
                "BaseColor",
                "Rendering",
                PropertyType::Vector3,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const PrimitiveComponent&>(object)
                        .material().baseColor;
                },
                [](Object& object, const PropertyValue& value) {
                    auto* component =
                        dynamic_cast<PrimitiveComponent*>(&object);
                    const auto* color = std::get_if<glm::vec3>(&value);
                    if (component == nullptr || color == nullptr) {
                        return false;
                    }
                    MaterialInstance material = component->material();
                    material.baseColor = *color;
                    component->setMaterial(material);
                    return true;
                },
            },
        },
    });
    registry.registerType({
        "StaticMeshComponent",
        &primitive,
        [](Object* outer, std::string name) {
            return std::make_unique<StaticMeshComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {},
    });
    registry.registerType({
        "BoxComponent",
        &primitive,
        [](Object* outer, std::string name) {
            return std::make_unique<BoxComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            {
                "Extent",
                "Collision",
                PropertyType::Vector3,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const BoxComponent&>(object).extent();
                },
                [](Object& object, const PropertyValue& value) {
                    auto* box = dynamic_cast<BoxComponent*>(&object);
                    const auto* extent = std::get_if<glm::vec3>(&value);
                    if (box == nullptr || extent == nullptr) {
                        return false;
                    }
                    box->setExtent(*extent);
                    return true;
                },
            },
            property<BoxComponent, bool>(
                "CollisionEnabled",
                "Collision",
                PropertyType::Boolean,
                &BoxComponent::collisionEnabled,
                &BoxComponent::setCollisionEnabled
            ),
            property<BoxComponent, bool>(
                "DrawDebug",
                "Collision",
                PropertyType::Boolean,
                &BoxComponent::drawDebug,
                &BoxComponent::setDrawDebug
            ),
            {
                "ObjectChannel",
                "Collision",
                PropertyType::Integer,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return static_cast<int>(
                        dynamic_cast<const BoxComponent&>(object)
                            .objectChannel()
                    );
                },
                [](Object& object, const PropertyValue& value) {
                    auto* box = dynamic_cast<BoxComponent*>(&object);
                    const auto* channel = std::get_if<int>(&value);
                    if (box == nullptr || channel == nullptr ||
                        *channel < 0 ||
                        *channel >=
                            static_cast<int>(CollisionChannel::Count)) {
                        return false;
                    }
                    box->setObjectChannel(
                        static_cast<CollisionChannel>(*channel)
                    );
                    return true;
                },
            },
        },
    });
    registry.registerType({
        "CameraComponent",
        &sceneComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<CameraComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<CameraComponent, float>(
                "FieldOfView",
                "Camera",
                PropertyType::Float,
                &CameraComponent::fieldOfView,
                &CameraComponent::setFieldOfView
            ),
            property<CameraComponent, bool>(
                "Active",
                "Camera",
                PropertyType::Boolean,
                &CameraComponent::active,
                &CameraComponent::setActive
            ),
        },
    });
    registry.registerType({
        "SpringArmComponent",
        &sceneComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<SpringArmComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<SpringArmComponent, float>(
                "TargetArmLength",
                "Camera",
                PropertyType::Float,
                &SpringArmComponent::targetArmLength,
                &SpringArmComponent::setTargetArmLength
            ),
        },
    });
    registry.registerType({
        "DirectionalLightComponent",
        &sceneComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<DirectionalLightComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            {
                "Color",
                "Light",
                PropertyType::Vector3,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const DirectionalLightComponent&>(object)
                        .color();
                },
                [](Object& object, const PropertyValue& value) {
                    auto* light =
                        dynamic_cast<DirectionalLightComponent*>(&object);
                    const auto* color = std::get_if<glm::vec3>(&value);
                    if (light == nullptr || color == nullptr) {
                        return false;
                    }
                    light->setColor(*color);
                    return true;
                },
            },
            property<DirectionalLightComponent, float>(
                "Intensity",
                "Light",
                PropertyType::Float,
                &DirectionalLightComponent::intensity,
                &DirectionalLightComponent::setIntensity
            ),
        },
    });
    const TypeDescriptor& pawn = registry.registerType({
        "Pawn",
        &actor,
        [](Object* outer, std::string name) {
            return std::make_unique<Pawn>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {},
    });
    registry.registerType({
        "Character",
        &pawn,
        [](Object* outer, std::string name) {
            return std::make_unique<Character>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {
            property<Character, float>(
                "MoveSpeed",
                "Character",
                PropertyType::Float,
                &Character::moveSpeed,
                &Character::setMoveSpeed
            ),
        },
    });
    const TypeDescriptor& controller = registry.registerType({
        "Controller",
        &actor,
        [](Object* outer, std::string name) {
            return std::make_unique<Controller>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {
            property<Controller, Guid>(
                "Pawn",
                "Controller",
                PropertyType::Guid,
                &Controller::pawnGuid,
                &Controller::setPawnGuid
            ),
        },
    });
    registry.registerType({
        "PlayerController",
        &controller,
        [](Object* outer, std::string name) {
            return std::make_unique<PlayerController>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {},
    });
    registry.registerType({
        "PlayerStart",
        &actor,
        [](Object* outer, std::string name) {
            return std::make_unique<PlayerStart>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {},
    });
    registry.registerType({
        "GameMode",
        &actor,
        [](Object* outer, std::string name) {
            return std::make_unique<GameMode>(
                std::move(name),
                dynamic_cast<World*>(outer)
            );
        },
        {},
    });
    registry.registerType({
        "HealthComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<HealthComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<HealthComponent, float>(
                "MaxHealth",
                "Combat",
                PropertyType::Float,
                &HealthComponent::maxHealth,
                &HealthComponent::setMaxHealth
            ),
            property<HealthComponent, float>(
                "CurrentHealth",
                "Combat",
                PropertyType::Float,
                &HealthComponent::currentHealth,
                &HealthComponent::setCurrentHealth
            ),
        },
    });
    registry.registerType({
        "ProjectileComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<ProjectileComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<ProjectileComponent, float>(
                "Damage",
                "Combat",
                PropertyType::Float,
                &ProjectileComponent::damage,
                &ProjectileComponent::setDamage
            ),
            property<ProjectileComponent, float>(
                "Lifetime",
                "Combat",
                PropertyType::Float,
                &ProjectileComponent::lifetime,
                &ProjectileComponent::setLifetime
            ),
        },
    });
    registry.registerType({
        "CombatComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<CombatComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            property<CombatComponent, float>(
                "ProjectileSpeed",
                "Combat",
                PropertyType::Float,
                &CombatComponent::projectileSpeed,
                &CombatComponent::setProjectileSpeed
            ),
            property<CombatComponent, float>(
                "ProjectileDamage",
                "Combat",
                PropertyType::Float,
                &CombatComponent::projectileDamage,
                &CombatComponent::setProjectileDamage
            ),
            property<CombatComponent, float>(
                "ProjectileLifetime",
                "Combat",
                PropertyType::Float,
                &CombatComponent::projectileLifetime,
                &CombatComponent::setProjectileLifetime
            ),
        },
    });
    registry.registerType({
        "RigidBodyComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<RigidBodyComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            {
                "Velocity",
                "Physics",
                PropertyType::Vector3,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const RigidBodyComponent&>(object)
                        .velocity();
                },
                [](Object& object, const PropertyValue& value) {
                    auto* rigidBody = dynamic_cast<RigidBodyComponent*>(&object);
                    const auto* velocity = std::get_if<glm::vec3>(&value);
                    if (rigidBody == nullptr || velocity == nullptr) {
                        return false;
                    }
                    rigidBody->setVelocity(*velocity);
                    return true;
                },
            },
            property<RigidBodyComponent, float>(
                "Mass",
                "Physics",
                PropertyType::Float,
                &RigidBodyComponent::mass,
                &RigidBodyComponent::setMass
            ),
            property<RigidBodyComponent, bool>(
                "Dynamic",
                "Physics",
                PropertyType::Boolean,
                &RigidBodyComponent::dynamic,
                &RigidBodyComponent::setDynamic
            ),
            property<RigidBodyComponent, bool>(
                "GravityEnabled",
                "Physics",
                PropertyType::Boolean,
                &RigidBodyComponent::gravityEnabled,
                &RigidBodyComponent::setGravityEnabled
            ),
        },
    });
    registry.registerType({
        "SkeletalAnimationComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<SkeletalAnimationComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            {
                "ClipJson",
                "Animation",
                PropertyType::String,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const SkeletalAnimationComponent&>(object)
                        .clipJson();
                },
                [](Object& object, const PropertyValue& value) {
                    auto* animation =
                        dynamic_cast<SkeletalAnimationComponent*>(&object);
                    const auto* clip = std::get_if<std::string>(&value);
                    if (animation == nullptr || clip == nullptr) {
                        return false;
                    }
                    animation->setClipJson(*clip);
                    return true;
                },
            },
            property<SkeletalAnimationComponent, bool>(
                "Playing",
                "Animation",
                PropertyType::Boolean,
                &SkeletalAnimationComponent::playing,
                &SkeletalAnimationComponent::setPlaying
            ),
            property<SkeletalAnimationComponent, float>(
                "PlaybackTime",
                "Animation",
                PropertyType::Float,
                &SkeletalAnimationComponent::playbackTime,
                &SkeletalAnimationComponent::setPlaybackTime
            ),
        },
    });
    registry.registerType({
        "BlueprintComponent",
        &actorComponent,
        [](Object* outer, std::string name) {
            return std::make_unique<BlueprintComponent>(
                std::move(name),
                dynamic_cast<Actor*>(outer)
            );
        },
        {
            {
                "GraphJson",
                "Blueprint",
                PropertyType::String,
                kEditSave,
                [](const Object& object) -> PropertyValue {
                    return dynamic_cast<const BlueprintComponent&>(object)
                        .graphJson();
                },
                [](Object& object, const PropertyValue& value) {
                    auto* blueprint = dynamic_cast<BlueprintComponent*>(&object);
                    const auto* graph = std::get_if<std::string>(&value);
                    if (blueprint == nullptr || graph == nullptr) {
                        return false;
                    }
                    blueprint->setGraphJson(*graph);
                    return true;
                },
            },
        },
    });
    registry.registerType({
        "GameInstance",
        &object,
        [](Object*, std::string) {
            return std::make_unique<GameInstance>();
        },
        {},
    });

    (void)world;
}

} // namespace engine
