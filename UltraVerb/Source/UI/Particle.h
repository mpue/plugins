/*
  ==============================================================================

    Particle.h
    Created: 3 Jun 2024 12:42:01pm
    Author:  mpue

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

using namespace juce;


class Particle
{
public:
    Particle() : position({ 0.0f, 0.0f }), velocity({ 0.0f, 0.0f }), size(1.0f), color(Colours::white), lifetime(1.0f), age(0.0f) {}

    void update(float deltaTime)
    {
        position += velocity * deltaTime;
        velocity *= 0.99f; // Dämpfung
        age += deltaTime;
    }

    void draw(Graphics& g) const
    {
        float alpha = jmax(0.0f, 1.0f - age / lifetime);
        g.setColour(color.withAlpha(alpha));
        g.fillEllipse(position.x - size * 0.5f, position.y - size * 0.5f, size, size);
    }

    bool isAlive() const
    {
        return age < lifetime;
    }

    Point<float> position;
    Point<float> velocity;
    float size;
    Colour color;
    float lifetime;
    float age;
};
