//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
// INF01047 Fundamentos de Computação Gráfica 2017/1
//               Prof. Eduardo Gastal
//
//                   TRABALHO FINAL
//              Felipe Bertoldo & Otávio Jacobi

#include <cstdio>
#include <cstdlib>
#include <cmath>

// Headers abaixo são específicos de C++
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

#include <stdio.h>
#include <irrKlang.h>

// include console I/O methods (conio.h for windows, our wrapper in linux)
#if defined(WIN32)
#include <conio.h>
#else
#include "../common/conio.h"
#endif

using namespace irrklang;
//#pragma comment(lib, "irrKlang.lib") // link with irrKlang.dll

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"

#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

// Estrutura que representa um modelo geométrico .obj
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando modelo \"%s\"... ", filename);

        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        printf("OK.\n");
    }
};


// Declaração de várias funções utilizadas em main().
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging
int check_wall_colision();
void check_box_colision();
void TextRendering_Count(GLFWwindow* window);
void TextRendering_ShowPontuacao(GLFWwindow* window);
void TextRendering_ShowTimeOut(GLFWwindow* window);
void TextRendering_GameOver(GLFWwindow* window);

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);


// Funções que controlam a lógica "não-trivial" do programa
void do_car_movement(int colision);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    void*        first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    int          num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário (map)
std::map<std::string, SceneObject> g_VirtualScene;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Para a velocidade não ser diferente em diferentes placas de vídeo
GLfloat deltaTime = 0.0f;	// Time between current frame and last frame
GLfloat lastFrame = 0.0f;  	// Time of last frame

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Variáveis que definem a câmera em coordenadas esféricas, veja função CursorPosCallback()).
float g_CameraTheta = 0.0f; // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = 0.0f;   // Ângulo em relação ao eixo Y
float g_CameraDistance = 5.0f; // Distância da câmera para a origem

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;


// Variaveis relativas ao objeto carro principal
// Car Position
glm::vec4 g_Car_Position = glm::vec4(45.0f,-1.0f,-45.0f,1.0f);
// Rotação "pitch" do carro
float g_Car_Pitch = 0.0f;
// Vetor da frente do carro
glm::vec4 g_Car_Front = glm::vec4(cos(g_CameraPhi)*sin(g_CameraTheta),
                                        sin(g_CameraPhi),
                                        cos(g_CameraPhi)*cos(g_CameraTheta),
                                        0.0f);


// Teclas pressionadas
bool key_w_pressed = false;
bool key_a_pressed = false;
bool key_s_pressed = false;
bool key_d_pressed = false;

bool space_pressed = false;

bool camera_type = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint vertex_shader_id;
GLuint fragment_shader_id;
GLuint program_id = 0;
GLint model_uniform;
GLint view_uniform;
GLint projection_uniform;
GLint object_id_uniform;
GLint bbox_min_uniform;
GLint bbox_max_uniform;
// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

float g_Car_aceleration = 1.0f;
float g_Car_backAceleration = 1.0f;

float g_AngleY = 0.0f;

int main_points = 0;

GLfloat carSpeed;
GLfloat carBackSpeed;


#define BOX_AMT 40
#define CLOUD_AMT 200

glm::vec3 coord_vec[BOX_AMT];
glm::vec3 coord_vec_sky[CLOUD_AMT];

const int time_out = 60;

bool shouldClose = false;

ISoundEngine* engine = createIrrKlangDevice();


int main(int argc, const char* argv[])
{

	if (!engine)
	{
		printf("Could not startup engine\n");
		return 0; // error starting up the engine
	}
    //engine->play2D("../../media/ophelia.mp3", true);
    engine->play2D("../../media/race_start.wav");
    engine->play2D("../../media/playback.wav", true);

    srand(time(NULL));
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com a OpenGL.

    int x_signal;
    int y_signal;
    int x_coord;
    int y_coord;
    int z_coord;

    for (int i =0; i <BOX_AMT; i++) {

        x_signal= (rand()%2 *2)-1;
        y_signal = (rand()%2 *2)-1;
        x_coord = x_signal*(rand() % 26 + 20);
        y_coord = y_signal*(rand() % 26 + 20);

        coord_vec[i].x = x_coord;
        coord_vec[i].y = y_coord;
        coord_vec[i].z = 1;
    }

    for (int i =0; i <CLOUD_AMT; i++) {

        x_signal= (rand()%2 *2)-1;
        y_signal = (rand()%2 *2)-1;
        x_coord =  x_signal*(rand() % 100 );
        z_coord =  y_signal*(rand() % 100 );
        y_coord = ((rand() % 100) + 12 );

        coord_vec_sky[i].x = x_coord;
        coord_vec_sky[i].y = y_coord;
        coord_vec_sky[i].z = z_coord;
    }

    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Pedimos apra utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas da OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional
    GLFWwindow* window;
    window = glfwCreateWindow(800, 600, "INFRun", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas pela OpenGL 3.3, utilizando a
    // biblioteca GLAD. Veja
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for redimensionada
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados para renderização.

    LoadShadersFromFiles();

    LoadTextureImage("./data/Brick_Wall_03.jpg");      // TextureImage0
    LoadTextureImage("./data/mk_kart/E_main.png");
    LoadTextureImage("./data/bricks.jpg");
    LoadTextureImage("./data/bricks.jpg");
    //LoadTextureImage("./data/tc-earth_daymap_surface.jpg");
    LoadTextureImage("./data/grama.jpg");
    LoadTextureImage("./data/cow.jpg");
    LoadTextureImage("./data/box.jpg");



    ObjModel planemodel("./data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel mariomodel("./data/mk_kart/mk_kart.obj");
    ComputeNormals(&mariomodel);
    BuildTrianglesAndAddToVirtualScene(&mariomodel);

    ObjModel spheremodel("./data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel cowmodel("./data/cow.obj");
    ComputeNormals(&cowmodel);
    BuildTrianglesAndAddToVirtualScene(&cowmodel);

    ObjModel cubemodel("./data/cube.obj");
    ComputeNormals(&cubemodel);
    BuildTrianglesAndAddToVirtualScene(&cubemodel);

    ObjModel cilindermodel("./data/cilinder.obj");
    ComputeNormals(&cilindermodel);
    BuildTrianglesAndAddToVirtualScene(&cilindermodel);

    //PrintObjModelInfo(&mariomodel);

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Variáveis auxiliares utilizadas para chamada à função TextRendering_ShowModelViewProjection()
    glm::mat4 the_projection;
    glm::mat4 the_model;
    glm::mat4 the_view;

    // Ficamos em loop, renderizando, até que o usuário feche a janela
    while (!shouldClose)
    {
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima, e também resetamos o Z-buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima
        glUseProgram(program_id);

        // Manter a mesma velocidade em diferentes sistemas (GPUS)
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Computamos a posição da câmera utilizando coordenadas esféricas veja as funções CursorPosCallback() e ScrollCallback().
        glm::mat4 view;
        if (camera_type) {
            float r = g_CameraDistance;
            //float y = r*sin(g_CameraPhi);
            //float z = r*cos(g_CameraPhi)*cos(g_CameraTheta);
            //float x = r*cos(g_CameraPhi)*sin(g_CameraTheta);

            // Abaixo definimos as varáveis que efetivamente definem a câmera virtual.

            glm::vec4 camera_lookat_l    = glm::vec4(g_Car_Position.x,g_Car_Position.y,g_Car_Position.z,1.0f); // Ponto "l", para onde a câmera (look-at) estará sempre olhando
            glm::vec4 oposite_escalated  = glm::vec4(r*g_Car_Front.x, r*g_Car_Front.y,r*g_Car_Front.z, 0.0f);
            glm::vec4 camera_position_c  = g_Car_Position - oposite_escalated + glm::vec4(0.0f,2.0f,0.0f,0.0f); // Ponto "c", centro da câmera
            glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c; // Vetor "view", sentido para onde a câmera está virada
            glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); // Vetor "up"

            // Computamos a matriz "View" e a matriz de Projeção.
            view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);
        }
        else {

            // Abaixo definimos as varáveis que efetivamente definem a câmera virtual.
            glm::vec4 camera_position_c  = glm::vec4(g_Car_Position.x + 2.5f * deltaTime*g_Car_Front.x *g_Car_aceleration, g_Car_Position.y + 0.85, g_Car_Position.z + 2.5f * deltaTime*g_Car_Front.z *g_Car_aceleration ,1.0f); // Ponto "c", centro da câmera
            //glm::vec4 camera_lookat_l    = glm::vec4(0.0f,0.0f,0.0f,1.0f); // Ponto "l", para onde a câmera (look-at) estará sempre olhando
            glm::vec4 camera_view_vector = g_Car_Front; // Vetor "view", sentido para onde a câmera está virada
            glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); // Vetor "up"

            // Computamos a matriz "View" e a matriz de Projeção.
            view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

        }
        glm::mat4 projection;

        float nearplane = -0.1f;  // Posição do "near plane"
        float farplane  = -600.0f; // Posição do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV)
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            float t = 1.5f*g_CameraDistance/2.5f;
            float b = -t;
            float r = t*g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));



        check_box_colision();
        int colision = check_wall_colision();

        if (glfwGetTime() > 3.61f && glfwGetTime() <= time_out+3.61f)
            do_car_movement(colision);

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

        model = Matrix_Translate(0.0f, -1.0f,0.0f)
              * Matrix_Scale(50.0f,50.0f,50.0f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, PLANE);
        DrawVirtualObject("plane");


        model = Matrix_Translate(g_Car_Position.x, g_Car_Position.y, g_Car_Position.z)
              * Matrix_Rotate_Y(g_Car_Pitch);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, MARIO);
        DrawVirtualObject("mario");

        model = Matrix_Translate(0.0f, -113.0f, 0.0f)
              * Matrix_Scale(120.0f, 120.0f, 120.0f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, CENTRAL_SPHERE);
        DrawVirtualObject("sphere");


        glDisable(GL_CULL_FACE);
        model = Matrix_Translate(0.0f, -250.0f, 0.0f)
              * Matrix_Scale(500.0f, 500.0f, 500.0f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, SPHERE);
        DrawVirtualObject("sphere");

        model = Matrix_Translate(0.0f, -48.75f, 50.0f)
              * Matrix_Scale(50.0f,50.0f,50.0f)
              * Matrix_Rotate_X(M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BORDER);
        DrawVirtualObject("plane");


        model = Matrix_Translate(0.0f,  -48.75f,-50.0f)
              * Matrix_Scale(50.0f,50.0f,50.0f)
              * Matrix_Rotate_X(M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BORDER);
        DrawVirtualObject("plane");

        model = Matrix_Translate(50.0f,  -48.75f,0.0f)
              * Matrix_Scale(50.0f,50.0f,50.0f)
              * Matrix_Rotate_Y(M_PI_2)
              * Matrix_Rotate_X(M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BORDER);
        DrawVirtualObject("plane");

        model = Matrix_Translate(-50.0f,  -48.75f,0.0f)
              * Matrix_Scale(50.0f,50.0f,50.0f)
              * Matrix_Rotate_Y(M_PI_2)
              * Matrix_Rotate_X(M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BORDER);
        DrawVirtualObject("plane");

        model = Matrix_Translate(45.0f, 1.5f, -42.0f)
              * Matrix_Scale(3.0f,0.5f,1.0f)
              * Matrix_Rotate_X(M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, REGULAR_COW);
        DrawVirtualObject("plane");

        glEnable(GL_CULL_FACE);

        model = Matrix_Translate(25.0f,0.55f, 25.0f)
            * Matrix_Rotate_Y(-M_PI_2/2)
            * Matrix_Rotate(-M_PI_2/6, glm::vec4(1.0f, 0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, REGULAR_COW);
        DrawVirtualObject("cow");

        for (int i=0; i < BOX_AMT; i ++) {

            if (coord_vec[i].z == 1) {
                model = Matrix_Translate(coord_vec[i].x, 0.0f, coord_vec[i].y)
                      * Matrix_Scale(0.35f,0.35f,0.35f)
                      * Matrix_Rotate(g_AngleY + (float)glfwGetTime() * 1.5f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
                glUniform1i(object_id_uniform, BOX);
                DrawVirtualObject("cube");
            }
        }

        for (int i=0; i < CLOUD_AMT; i++) {

            model = Matrix_Translate(coord_vec_sky[i].x, coord_vec_sky[i].y,  coord_vec_sky[i].z);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
            glUniform1i(object_id_uniform, REGULAR_COW);
            DrawVirtualObject("cilinder");
        }

        model = Matrix_Translate(42.0f, 0.0f, -42.0f)
              * Matrix_Scale(0.10f,2.0f,0.10f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, ESTACA);
        DrawVirtualObject("cube");

        model = Matrix_Translate(48.0f, 0.0f, -42.0f)
              * Matrix_Scale(0.10f,2.0f,0.10f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, ESTACA);
        DrawVirtualObject("cube");


        model = Matrix_Translate(48.0f, 0.2f, -20.0f)
              * Matrix_Scale(2.0f,2.0f,2.0f)
              * Matrix_Rotate_Y(-M_PI_2);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE, glm::value_ptr(model));
        glUniform1i(object_id_uniform, GOLDEN_COW);
        DrawVirtualObject("cow");

        // Imprimimos na tela informação sobre os frames per second
        //TextRendering_ShowFramesPerSecond(window);
        shouldClose = glfwWindowShouldClose(window);

        if (glfwGetTime() >= time_out + 3.61 + 5)
            shouldClose = true;


        TextRendering_Count(window);
        TextRendering_ShowPontuacao(window);
        TextRendering_ShowTimeOut(window);
        TextRendering_GameOver(window);



        glfwSwapBuffers(window);

        glfwPollEvents();
    }
    engine->drop(); // delete engine
    glfwTerminate();
    return 0;
}

void check_box_colision() {

    for (int i=0; i < BOX_AMT; i++) {
        double dx = (g_Car_Position.x-coord_vec[i].x)*(g_Car_Position.x-coord_vec[i].x);
        double dz = (g_Car_Position.z-coord_vec[i].y)*(g_Car_Position.z-coord_vec[i].y); // O Y É O Z
        double distance = sqrt(dx+dz);

        if ( distance < 0.8f) {

            if (coord_vec[i].z == 1) {
                main_points++;
                engine->play2D("../../media/box_colision.wav");
            }

            coord_vec[i].z = 0;
        }
    }

}

int check_wall_colision() {
    // printf("Min: (%f, %f, %f) \n", g_VirtualScene["mario"].bbox_min.x, g_VirtualScene["mario"].bbox_min.y, g_VirtualScene["mario"].bbox_min.z);
    // printf("Max: (%f, %f, %f) \n", g_VirtualScene["mario"].bbox_max.x, g_VirtualScene["mario"].bbox_max.y, g_VirtualScene["mario"].bbox_max.z);
    //printf("Max: (%f, %f, %f) \n", g_Car_Position.x, g_Car_Position.y, g_Car_Position.z);

    if ( g_Car_Position.x +0.58 >= 50 )
        return 1;

    if ( g_Car_Position.x -0.58 <= -50 )
        return 2;

    if ( g_Car_Position.z +0.58 >= 50 )
        return 3;

    if ( g_Car_Position.z -0.58 <= -50 )
        return 4;



    return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slide 160 do documento "Aula_20_e_21_Mapeamento_de_Texturas.pdf"
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura. Falaremos sobre eles em uma próxima aula.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene.
void DrawVirtualObject(const char* object_name)
{

    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene[object_name].first_index
    );

    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão utilizados para renderização.
void LoadShadersFromFiles()
{
    vertex_shader_id = LoadShader_Vertex("./src/shader_vertex.glsl");
    fragment_shader_id = LoadShader_Fragment("./src/shader_fragment.glsl");

    if ( program_id != 0 )
        glDeleteProgram(program_id);

    program_id = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    model_uniform           = glGetUniformLocation(program_id, "model"); // Variável da matriz "model"
    view_uniform            = glGetUniformLocation(program_id, "view"); // Variável da matriz "view" em shader_vertex.glsl
    projection_uniform      = glGetUniformLocation(program_id, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    object_id_uniform       = glGetUniformLocation(program_id, "object_id"); // Variável "object_id" em shader_fragment.glsl

    bbox_min_uniform        = glGetUniformLocation(program_id, "bbox_min");
    bbox_max_uniform        = glGetUniformLocation(program_id, "bbox_max");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(program_id);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage4"), 4);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage5"), 5);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage6"), 6);

    glUseProgram(0);
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    // if ( !model->attrib.normals.empty() )
    //     return;

    size_t num_vertices = model->attrib.vertices.size() / 3;

    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            glm::vec4  vertices[3];
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
            }

            const glm::vec4  a = vertices[0];
            const glm::vec4  b = vertices[1];
            const glm::vec4  c = vertices[2];

            const glm::vec4  n = crossproduct((b-a),(c-a))/norm(crossproduct((b-a),(c-a)));

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                num_triangles_per_vertex[idx.vertex_index] += 1;
                vertex_normals[idx.vertex_index] += n;
                model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = idx.vertex_index;
            }
        }
    }

    model->attrib.normals.resize( 3*num_vertices );

    for (size_t i = 0; i < vertex_normals.size(); ++i)
    {
        glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
        n /= norm(n);
        model->attrib.normals[3*i + 0] = n.x;
        model->attrib.normals[3*i + 1] = n.y;
        model->attrib.normals[3*i + 2] = n.z;
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                if ( model->attrib.normals.size() >= (size_t)3*idx.normal_index )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( model->attrib.texcoords.size() >= (size_t)3*idx.texcoord_index )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = (void*)first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!

    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatóriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}

// Definição da função que será chamada sempre que a janela for redimensionada,
// por consequência alterando o tamanho do "framebuffer" (região de memória
// onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{

    glViewport(0, 0, width, height);
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (g_LeftMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da câmera com os deslocamentos
        g_CameraTheta -= 0.01f*dx;
        g_CameraPhi   += 0.01f*dy;

        // Em coordenadas esféricas, o ângulo phi deve ficar entre -pi/2 e +pi/2.
        float phimax = 3.141592f/2;
        float phimin = -phimax;

        if (g_CameraPhi > phimax)
            g_CameraPhi = phimax;

        if (g_CameraPhi < phimin)
            g_CameraPhi = phimin;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_RightMouseButtonPressed)
    {

    }

    if (g_MiddleMouseButtonPressed)
    {

    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f*yoffset;

    if (g_CameraDistance < 0.0f)
        g_CameraDistance = 0.0f;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);


    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }


    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        camera_type = !camera_type;
    }

    if(action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_W) key_w_pressed = true;
        if (key == GLFW_KEY_A) key_a_pressed = true;
        if (key == GLFW_KEY_S) key_s_pressed = true;
        if (key == GLFW_KEY_D) key_d_pressed = true;
        if (key == GLFW_KEY_SPACE) space_pressed = true;

    }
    else if(action == GLFW_RELEASE)
    {
        if (key == GLFW_KEY_W) key_w_pressed = false;
        if (key == GLFW_KEY_A) key_a_pressed = false;
        if (key == GLFW_KEY_S) key_s_pressed = false;
        if (key == GLFW_KEY_D) key_d_pressed = false;
        if (key == GLFW_KEY_SPACE) space_pressed = false;
    }

}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

void TextRendering_Count(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    //static float old_seconds = (float)glfwGetTime();
    static char  buffer[20];
    static int   numchars = 7;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    if (seconds <= 1.2f) {
        strcpy(buffer, " 3");
    }
    else if (seconds <= 2.4f) {
        strcpy(buffer, " 2");
    }
    else if (seconds <= 3.6f) {
        strcpy(buffer, " 1");
    }
    else if (seconds <= 4.8f) {
        strcpy(buffer, "GO!");
    }
    else {
        strcpy(buffer, "");
    }


    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    //TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth - 0.9, 1.0f-lineheight-0.5, 4.0f);
}

void TextRendering_ShowPontuacao(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Points : %d\n", main_points);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

void TextRendering_ShowTimeOut(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];


    int showTime = (int)glfwGetTime();

    if (showTime >= 3) {
        showTime -= 3;
    }
    else if (showTime >= time_out) {
        showTime = time_out;
    }
    else {
        showTime = 0;
    }

    static int   numchars = 12;

    snprintf(buffer, 80, "Tempo : %d/%d\n", showTime, time_out);

    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth - 0.025f, -1.0f+2*pad/10, 1.0f);
}

void TextRendering_GameOver(GLFWwindow* window) {

    if ( !g_ShowInfoText )
        return;

    char buffer[80];
    char gastal[80];

    static int numchars;
    if (glfwGetTime() >= time_out+3.61f) {

        if(main_points == BOX_AMT) {
            numchars = 11;
            snprintf(buffer, 80, "You WON !!!");

            float lineheight = TextRendering_LineHeight(window);
            float charwidth = TextRendering_CharWidth(window);

            //TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
            TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth - 0.9, 1.0f-lineheight-0.5, 2.0f);
        }
        else {
            numchars = 10;
            snprintf(buffer, 80, "You LOST");
            snprintf(gastal, 80, "Points: %d", main_points);

            float lineheight = TextRendering_LineHeight(window);
            float charwidth = TextRendering_CharWidth(window);

            //TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
            TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth - 0.9, 1.0f-lineheight-0.5, 2.0f);
            TextRendering_PrintString(window, gastal, 1.0f-(numchars + 1)*charwidth - 0.9, 1.0f-lineheight-0.75, 2.0f);
        }
    }
    else {
        numchars = 1;
        snprintf(buffer, 80, " ");

        float lineheight = TextRendering_LineHeight(window);
        float charwidth = TextRendering_CharWidth(window);

        //TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
        TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth - 0.9, 1.0f-lineheight-0.5, 2.0f);
    }





}


// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

void do_car_movement(int colision)
{

    bool w_enable = true;

    if (colision != 0) {
        carSpeed = 0.0f;
        g_Car_aceleration = 1.0f;
    }

    if (colision == 1) {
        g_Car_Position.x -= 1.5f * 2.5f * deltaTime ;
        w_enable=false;
    }

    if (colision == 2) {
        g_Car_Position.x += 1.5f * 2.5f * deltaTime ;
        w_enable=false;
    }

    if (colision == 3) {
        g_Car_Position.z -= 1.5f * 2.5f * deltaTime ;
        w_enable=false;
    }

    if (colision == 4) {
        g_Car_Position.z += 1.5f * 2.5f * deltaTime ;
        w_enable=false;
    }


    if (sqrt(pow(g_Car_Position.x,2) + pow(g_Car_Position.z,2)) < 40 ) {
        if ( g_Car_aceleration <= 1.0f ) g_Car_aceleration = 1.0f;
        else {
            g_Car_aceleration -= 2.00f *  deltaTime * 40;
            g_Car_Position  += carSpeed * g_Car_Front;
        }
    }

    if (sqrt(pow(g_Car_Position.x,2) + pow(g_Car_Position.z,2)) < 38 ) {
            carSpeed = 0.0f;
            g_Car_aceleration = 1.0f;
            carBackSpeed = 0.0f;
            g_Car_backAceleration = 1.0f;
            g_Car_Position.x -= 5.0f * 2.5f * deltaTime * g_Car_Front.x;
            g_Car_Position.z -= 5.0f * 2.5f * deltaTime * g_Car_Front.z;
            //printf("(%f, %f, %f)\n", g_Car_Front.x,  g_Car_Front.y,  g_Car_Front.z);
    }


    carSpeed = 2.5f * deltaTime * g_Car_aceleration;
    carBackSpeed = 1.2f * deltaTime * g_Car_backAceleration;


    if (space_pressed) {
        carSpeed = 0.0f;
        g_Car_aceleration = 1.0f;
        carBackSpeed = 0.0f;
        g_Car_backAceleration = 1.0f;
    }


    if (key_w_pressed && w_enable)
    {
        if (g_Car_aceleration >= 8.5f) g_Car_aceleration = 8.5f;
        g_Car_aceleration += 0.05f * deltaTime * 30;
        g_Car_Position  += carSpeed * g_Car_Front;
    }
    else
    {
        if ( g_Car_aceleration <= 1.0f ) g_Car_aceleration = 1.0f;
        else {
            g_Car_aceleration -= 0.08f *  deltaTime * 40;
            g_Car_Position  += carSpeed * g_Car_Front;
        }
    }
    if (key_s_pressed)
    {
        if (g_Car_aceleration >= 6.0f) g_Car_aceleration = 6.0f;
        g_Car_backAceleration += 0.03f *  deltaTime * 10;
        g_Car_Position  -= carBackSpeed * g_Car_Front;
    }
    else
    {
        if ( g_Car_backAceleration <= 1.0f ) g_Car_backAceleration = 1.0f;
        else {
            g_Car_backAceleration -= 0.085f * deltaTime * 50;
            g_Car_Position  -= carSpeed * g_Car_Front;
        }

    }
    if (key_a_pressed)
    {
        g_Car_aceleration -= 0.05f * deltaTime * 10;
        g_Car_backAceleration -= 0.03f * deltaTime * 10;
        if ( g_Car_aceleration <= 1.0f ) g_Car_aceleration = 1.0f;
        if ( g_Car_backAceleration <= 1.0f ) g_Car_backAceleration = 1.0f;

        glm::vec4 tmp;

        tmp.x = sin(g_Car_Pitch);
        tmp.y = 0;
        tmp.z = cos(g_Car_Pitch);
        tmp.w = 0.0f;

        g_Car_Front = tmp/norm(tmp);

        g_Car_Pitch += 3.5f * deltaTime;

        if (g_Car_Pitch >= 2*3.141592)
            g_Car_Pitch = 0;
    }
    if (key_d_pressed)
    {
        g_Car_aceleration -= 0.05f * deltaTime*10;
        g_Car_backAceleration -= 0.03f * deltaTime*10;
        if ( g_Car_aceleration <= 1.0f ) g_Car_aceleration = 1.0f;
        if ( g_Car_backAceleration <= 1.0f ) g_Car_backAceleration = 1.0f;

        glm::vec4 tmp;

        tmp.x = sin(g_Car_Pitch);
        tmp.y = 0;
        tmp.z = cos(g_Car_Pitch);
        tmp.w = 0.0f;

        g_Car_Front = tmp/norm(tmp);

        g_Car_Pitch -= 3.5f * deltaTime;

        if (g_Car_Pitch <= -2*3.141592)
            g_Car_Pitch = 0;

    }
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :
