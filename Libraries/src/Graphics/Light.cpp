#include "Light.h"

struct alignas(16) Header {
    int size;
    int padding[3] = {0, 0, 0};

    lght::AmbientLightBlock ambientLight;
};




LightManager::LightManager() {
    this->ResetAmbientLight();
}

LightManager::LightManager(glm::vec3 color, float strength) {
    this->SetAmbientLight(color, strength);
}

LightManager::~LightManager() {
    this->Destroy();
}

void LightManager::initSSBO() { 
    this->LightSSBO.Destroy();
    this->LightSSBO.Initialize(sizeof(Header) + sizeof(lght::LightBlock) * this->lLight.size(), LIGHT_BINDING_POINT);
}

void LightManager::Destroy() { 
    this->LightSSBO.Destroy();
}

void LightManager::updateSSBO() {
    if (this->LightsChanged) {
        this->LightSSBO.ResizePreserveData(sizeof(Header) + sizeof(lght::LightBlock) * this->lLight.size());
    }

    if (this->LightsChanged || this->AmbientLightChanged) {
        Header hHeader = { .size = this->size, .ambientLight = this->ambientLight };
        this->LightSSBO.UploadData(&hHeader, sizeof(Header), 0);
        this->LightsChanged = false;
        this->AmbientLightChanged = false;
    }

    for (size_t i = 0; i < this->lLight.size(); i++) {
        if (!this->LightChanged[i]) continue;
        lght::LightBlock l = this->lLight[i];
        this->LightSSBO.UploadData(&l, sizeof(lght::LightBlock), i * sizeof(lght::LightBlock) + sizeof(Header));
        this->LightChanged[i] = false;
    } 
}

void LightManager::BindSSBO() { 
    this->LightSSBO.BindToPoint();
}


void LightManager::AddLight(lght::Light Light) {
    this->LightsChanged = true;
    this->lLight.push_back(Light);
    this->LightChanged.push_back(true);
    this->size++;
}

void LightManager::AddLight(std::vector<lght::Light> Light) {
    for (size_t i = 0; i < Light.size(); i++) {
        this->AddLight(Light[i]);
    }
}

void LightManager::SetAmbientLight(glm::vec3 color, float strength) {
    this->ambientLight = lght::AmbientLight(color, strength);
    this->AmbientLightChanged = true;
}

void LightManager::RemoveLight(size_t index) {
    if (index >= this->lLight.size()) return;

    this->LightsChanged = true;
    this->LightChanged[index] = true;
    this->lLight[index] = this->lLight.back();
    this->lLight.pop_back();
}


void LightManager::RemoveLight(lght::Light Light) {
    for (size_t i = 0; i < this->lLight.size(); i++) {
        if (this->lLight[i] == Light) {
            this->RemoveLight(i);
            return;
        }
    }
}

void LightManager::RemoveLight(std::vector<lght::Light> Light) {
    for (size_t i = 0; i < Light.size(); i++) {
        this->RemoveLight(Light[i]);
    }
}


void LightManager::ResetAmbientLight() {
    this->ambientLight = lght::AmbientLight(glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
    this->AmbientLightChanged = true;
}

void LightManager::SetLight(size_t index, lght::Light Light) {
    if (index >= this->lLight.size()) 
        throw std::out_of_range("Index out of range");
    this->lLight[index] = Light;
    this->LightChanged[index] = true;
}


lght::Light& LightManager::operator[](size_t index) { 
    index = index % this->lLight.size();
    return this->lLight[index]; 
}


lght::Light& LightManager::GetLight(size_t index) { 
    if (index >= this->lLight.size()) 
        throw std::out_of_range("Index out of range");
    return this->lLight[index]; 
}


std::vector<lght::Light> LightManager::GetLight() { 
    return this->lLight;
}

lght::Light& LightManager::GetAmbientLight() { 
    return this->ambientLight; 
}