/*
  ==============================================================================

    ParticleSystemComponent.h
    Created: 3 Jun 2024 12:41:39pm
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Particle.h"

using namespace juce;

class ParticleSystemComponent : public Component, public Timer
{
public:
    ParticleSystemComponent()
    {
        setOpaque(false);
        startTimerHz(60);
    }

    void timerCallback() override
    {
        const float deltaTime = 1.0f / 60.0f;
        for (auto* particle : particles)
        {
            particle->update(deltaTime);
        }
        removeDeadParticles();
        repaint();
    }

    void paint(Graphics & g) override
    {
        
        for (const auto* particle : particles)
        {
            particle->draw(g);
        }
    }

    void addParticle(const Point<float>&position, const Point<float>&velocity, float size, Colour color, float lifetime)
    {
        auto* p = new Particle();
        p->position = position;
        p->velocity = velocity;
        p->size = size;
        p->color = color;
        p->lifetime = lifetime;
        particles.add(p);
    }

private:
    OwnedArray<Particle> particles;

    void removeDeadParticles()
    {
        for (int i = particles.size(); --i >= 0;)
        {
            if (!particles[i]->isAlive())
            {
                particles.remove(i);
            }
        }
    }
};