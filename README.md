# Florb v1.3
Welcome to Florb, the Flower Orb!

# What is Florb?
Florb is an OpenGL application written by Eldridge M. Mount IV,
for the purpose of showcasing his wildflower photography.

Florb is fundamentally an orb which displays a collection of images,
generally flowers, in the center of a dark screen. Multiple effects may
be enabled and configured with parameters, all of which may be changed
via a JSON configuration file (florb.json).

## Effects
Effects add to the pizazz of a collection of flower images.

### Transition mode
Images may transition either instantly after a delay, or make a smooth
transition by blending from one into the next.

### Vignette
A smooth vignette effect may be enabled, achieved via configuration of
a radius and exponent.

### Spotlights
Spotlights provide a highly-configurable means for illuinating the orb,
with the ability to set direction, intensity, color, and motion. A
shininess factor sets the reflectivity on a global basis.

### Rim lighting
Rim lighting can provide either a subtle glow around the edges of the
orb, or a brilliant illumination which covers the entire sphere with
soft light. Strength, exponent, and animated color with configurable
speed may be set up.

### Dust motes
A collection of swirling dust motes can be configured by count, radius,
color, and maximum step size. Depending upon the desire of the user,
this effect can look like either a cloud of dust, or a snowglobe effect.

### Debugging modes
A couple of debugging modes have been left in the code in order to allow
simple diagnosis of coding and configuration issues.

#### Render mode
Rendering mode can be set to normal texture fill mode, or a line draw
mode, producing a skeletal view of the sphere with textures and effects
mapped onto it. This can be helpful with debugging geometry issues.

#### Specular mode
Specular reflections may be either rendered normally, or in debug mode.
In debug mode, image textures are not rendered, allowing only the specular
reflections to shine through. This is extremely useful in setting the
position, direction, and speed of spotlights.

# Conclusion
Not only does Florb involve the sedentary and geeky process of coding and
collating taxonomical metadata, ita also encourages active fieldwork in
the pursuit of ever-more beautiful species of flowers!

Happy Florbing!
