#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da cor de cada vértice, definidas em "shader_vertex.glsl" e
// "main.cpp".
in vec4 position_world;
in vec4 normal;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

uniform vec4 bbox_min;
uniform vec4 bbox_max;


// Identificador que define qual objeto está sendo desenhado no momento
#define SPHERE 0
#define BUNNY  1
#define PLANE  2
#define MARIO  3
#define BORDER 4
#define CENTRAL_SPHERE 5
#define REGULAR_COW 6
#define BOX 7
#define ESTACA 8
#define GOLDEN_COW 9
uniform int object_id;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;
uniform sampler2D TextureImage3;
uniform sampler2D TextureImage4;
uniform sampler2D TextureImage5;
uniform sampler2D TextureImage6;


// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec3 color;


// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923
void main()
{
    // Obtemos a posição da câmera utilizando a inversa da matriz que define o
    // sistema de coordenadas da câmera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // O fragmento atual é coberto por um ponto que percente à superfície de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
    // sistema de coordenadas global (World coordinates). Esta posição é obtida
    // através da interpolação, feita pelo rasterizador, da posição de cada
    // vértice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vértice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
    vec4 l = normalize(vec4(0.0,1.0,0.0,0.0));

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Vetor que define o sentido da reflexão especular ideal.
    vec4 r = -l + 2*n*(dot(n,l)); // PREENCHA AQUI o vetor de reflexão especular ideal

    // Parâmetros que definem as propriedades espectrais da superfície
    vec3 Kd; // Refletância difusa
    vec3 Ks; // Refletância especular
    vec3 Ka; // Refletância ambiente
    float q; // Expoente especular para o modelo de iluminação de Phong

    float U = 0.0;
    float V = 0.0;

    vec3 Kd1 = vec3(1.0f, 1.0f, 1.0f);
    vec3 Kd2 = vec3(1.0f, 1.0f, 1.0f);
    if ( object_id == SPHERE )
    {
        // PREENCHA AQUI
        // Propriedades espectrais da esfera
        Kd = vec3(0.0f, 0.794f, 1.0f);
        //Kd = vec3(0.678f, 0.847f, 1zz.0f);
        //Ks = vec3(0.0,1.0,0.0); //Aurora Boreal
        Ks = vec3(0.0,0.0, 0.0);
        Ka = Kd;
        q = 1.0;
    }
    else if ( object_id == GOLDEN_COW)
    {

        vec4 n = normalize(normal);

        // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
        vec4 l = normalize(vec4(-1.0,1.0,0.0,0.0));

        // Vetor que define o sentido da câmera em relação ao ponto atual.
        vec4 v = normalize(camera_position - p);

        // Vetor que define o sentido da reflexão especular ideal.
        vec4 r = -l + 2*n*(dot(n,l)); // PREENCHA AQUI o vetor de reflexão especular ideal

        // AQUI
        Kd = vec3(0.854f, 0.647f, 0.125f);
        Ks = vec3(1.0f, 1.0f, 1.0f);
        Ka = Kd/2;
        q = 16.0;

        float lambert_diffuse_term = max(0.0, dot(n,l)); // PREENCHA AQUI o termo difuso de Lambert

        // Termo especular utilizando o modelo de iluminação de Phong
        float phong_specular_term  = pow(max(0.0, dot(r,v)),q); // PREENCH AQUI o termo especular de Phong

        // Espectro da fonte de iluminação
        vec3 light_spectrum = vec3(1.0,1.0,1.0); // PREENCH AQUI o espectro da fonte de luz

        // Espectro da luz ambiente
        vec3 ambient_light_spectrum = vec3(0.2,0.2,0.2); // PREENCHA AQUI o espectro da luz ambiente

        // Cor final do fragmento calculada com uma combinação dos termos difuso,
        // especular, e ambiente. Veja slide 131 do documento "Aula_17_e_18_Modelos_de_Iluminacao.pdf".

        color =  Kd * light_spectrum * lambert_diffuse_term
              + Ka * ambient_light_spectrum
              + Ks * light_spectrum * phong_specular_term;
    }

    else if ( object_id == BOX) {

      if (position_model.x == -1 || position_model.x == 1) {
        U = (position_model.y + 1)/2;
        V = (position_model.z + 1)/2;
      }

      if (position_model.z == -1 || position_model.z == 1) {
        U = (position_model.y + 1)/2;
        V = (position_model.x + 1)/2;
      }

      if (position_model.y  == -1 || position_model.y == 1) {
        U = (position_model.z + 1)/2;
        V = (position_model.x + 1)/2;
      }

      vec3 Kd = texture(TextureImage6, vec2(U,V)).rgb;

      // Equação de Iluminação
      vec4 tmp = vec4(0.0f, -1.0f, 0.0f, 0.0f);
      float lambert = max(0,dot(n,tmp));

      color = Kd* (lambert + 0.5f);



    }
    else if ( object_id == CENTRAL_SPHERE) {

      vec4 bbox_center = (bbox_min + bbox_max) / 2.0;

      float pl = sqrt(position_model.x*position_model.x + position_model.y*position_model.y + position_model.z*position_model.z);
      float theta = atan(position_model.x, position_model.z );
      float phi = asin(position_model.y/pl);

      U = (theta+M_PI)/(2*M_PI);
      V = (phi+M_PI_2)/(M_PI);

      float range = 0.01f;

      float a = U;
      float b = V;

      while (a>range)
        a -= range;
      a /= range;


      while (b>range)
        b-=range;
      b=b/range;

      U=a;
      V=b;

      Kd2 = texture(TextureImage4, vec2(U,V)).rgb;

      // Equação de Iluminação
      float lambert = max(0,dot(n,l));

      color = Kd2* (lambert + 0.2);
    }
    else if ( object_id == BUNNY )
    {
        // PREENCHA AQUI
        // Propriedades espectrais do coelho
        Kd = vec3(0.08f, 0.4f, 0.8f);
        Ks = vec3(0.8f, 0.8f, 0.8f);
        Ka = Kd/2;
        q = 32.0;
    }

    else if ( object_id == ESTACA )
    {
        // PREENCHA AQUI
        // Propriedades espectrais do coelho
        Kd = vec3(0.08f, 0.4f, 0.8f);
        Ks = vec3(0.8f, 0.8f, 0.8f);
        Ka = Kd/2;
        q = 2.0;
    }

    else if ( object_id == BORDER )
    {

      float range = 0.035f;

      float a = texcoords.x;
      float b = texcoords.y;

      while (a>range)
        a -= range;
      a /= range;


      while (b>range)
        b-=range;
      b=b/range;

      U=a;
      V=b;

      Kd1 = texture(TextureImage3, vec2(U,V)).rgb;

      // Equação de Iluminação
      float lambert = max(0,dot(n,l));

      color = Kd1* (lambert + 1);
    }

    else if ( object_id == PLANE )
    {
        // PREENCHA AQUI
        // Propriedades espectrais do plano
        // Kd = vec3(0.2, 0.2, 0.2);
        // Ks = vec3(0.3, 0.3, 0.3);
        // Ka = vec3(0.0, 0.0, 0.0);
        // q = 20.0;

        // Coordenadas de textura do plano,i
        // obtidas do arquivo OBJ.
        float range = 0.025f;

        float a = texcoords.x;
        float b = texcoords.y;

        while (a>range)
          a -= range;
        a /= range;


        while (b>range)
          b-=range;
        b=b/range;

        U=a;
        V=b;

        vec3 Kd0 = texture(TextureImage0, vec2(U,V)).rgb;

        // Equação de Iluminação
        float lambert = max(0,dot(n,l));

        color = Kd0 * (lambert + 0.01);
    }

    else if ( object_id == MARIO)
    {
      // Kd = vec3(0.08, 0.4, 0.8);
      // Ks = vec3(0.8, 0.8, 0.8);
      // Ka = Kd/2;
      // q = 32.0;

      vec4 l_mario = normalize(vec4(0.0,1.0,1.0,0.0));

      U = texcoords.x;
      V = texcoords.y;
      vec3 Kd0 = texture(TextureImage1, vec2(U,V)).rgb;

      // Equação de Iluminação
      float lambert = max(0,dot(n,l_mario));

      color = Kd0 * (lambert + 0.01);

    }
    else if ( object_id == REGULAR_COW) {

      vec4 bbox_center = (bbox_min + bbox_max) / 2.0;

      float pl = sqrt(position_model.x*position_model.x + position_model.y*position_model.y + position_model.z*position_model.z);
      float theta = atan(position_model.x, position_model.z );
      float phi = asin(position_model.y/pl);

      U = (theta+M_PI)/(2*M_PI);
      V = (phi+M_PI_2)/(M_PI);

      vec3 Kd = texture(TextureImage5, vec2(U,V)).rgb;

      // Equação de Iluminação
      float lambert = max(0,dot(n,l));

      color = Kd* (lambert + 0.2);


    }
    else // Objeto desconhecido = preto
    {
        Kd = vec3(0.0,0.0,0.0);
        Ks = vec3(0.0,0.0,0.0);
        Ka = vec3(0.0,0.0,0.0);
        q = 1.0;
    }
    // Termo difuso utilizando a lei dos cossenos de Lambert
    if ( object_id != PLANE
      && object_id != MARIO
      && object_id != BORDER
      && object_id != CENTRAL_SPHERE
      && object_id != REGULAR_COW
      && object_id != BOX
      && object_id != GOLDEN_COW) {
      float lambert_diffuse_term = max(0.0, dot(n,l)); // PREENCHA AQUI o termo difuso de Lambert

      // Termo especular utilizando o modelo de iluminação de Phong
      float phong_specular_term  = pow(max(0.0, dot(r,v)),q); // PREENCH AQUI o termo especular de Phong

      // Espectro da fonte de iluminação
      vec3 light_spectrum = vec3(1.0,1.0,1.0); // PREENCH AQUI o espectro da fonte de luz

      // Espectro da luz ambiente
      vec3 ambient_light_spectrum = vec3(0.2,0.2,0.2); // PREENCHA AQUI o espectro da luz ambiente

      // Cor final do fragmento calculada com uma combinação dos termos difuso,
      // especular, e ambiente. Veja slide 131 do documento "Aula_17_e_18_Modelos_de_Iluminacao.pdf".

      color =  Kd * light_spectrum * lambert_diffuse_term
            + Ka * ambient_light_spectrum
            + Ks * light_spectrum * phong_specular_term;
      }


      // Cor final com correção gamma, considerando monitor sRGB.
      // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
      color = pow(color, vec3(1.0,1.0,1.0)/2.2);
}
