#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texSampler;
uniform bool hasTexture;
uniform vec4 solidColor;

void main()
{
    vec4 color = hasTexture ? texture(texSampler, TexCoord) : solidColor;
    if(hasTexture && color.a < 0.1)
        discard;

    FragColor = color;
}
