#pragma once

#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

using namespace Hyprutils::Memory;

#define UP Hyprutils::Memory::CUniquePointer
#define SP Hyprutils::Memory::CSharedPointer
#define WP Hyprutils::Memory::CWeakPointer
