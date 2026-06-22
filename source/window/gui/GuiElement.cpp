#include "ship/window/gui/GuiElement.h"

namespace Ship {

GuiElement::GuiElement(bool isVisible) : mIsVisible(isVisible), mIsInitialized(false) {}

GuiElement::GuiElement() : GuiElement(false) {}

GuiElement::~GuiElement() = default;

void GuiElement::Init() {
    if (!mIsInitialized) {
        InitElement();
        mIsInitialized = true;
    }
}

void GuiElement::Update() {
    UpdateElement();
}

void GuiElement::Show() {
    SetVisibility(true);
}

void GuiElement::Hide() {
    SetVisibility(false);
}

void GuiElement::ToggleVisibility() {
    SetVisibility(!mIsVisible);
}

bool GuiElement::IsVisible() {
    return mIsVisible;
}

bool GuiElement::IsInitialized() {
    return mIsInitialized;
}

void GuiElement::SetVisibility(bool visible) {
    mIsVisible = visible;
}

} // namespace Ship
