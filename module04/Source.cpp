#include <iostream>         // cout, cerr
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>      // Image loading Utility functions

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnOpengl/camera.h> // Camera class

using namespace std; // Standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
    const char* const WINDOW_TITLE = "Project - Rayyan Abdulmunib"; // Macro for window title

    // Variables for window width and height
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    // Stores the GL data relative to a given mesh
    struct GLMesh
    {
        GLuint vao;         // Handle for the vertex array object
        GLuint vbo;         // Handle for the vertex buffer object
        GLuint nVertices;    // Number of indices of the mesh
        GLuint nIndices;
        GLuint ebo;
    };

    // Main GLFW window
    GLFWwindow* gWindow = nullptr;
    // Triangle mesh data
    GLMesh gMesh;
    // Texture id
    GLuint gTextureId;
    // Shader program
    GLuint gProgramId;

    GLuint gHemTor;
    GLuint gPlane;
    GLuint gRollPin;
    GLuint gEggs;

    // Camera
    Camera gCamera(glm::vec3(0.0f, 0.0f, 3.0f));
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;
    bool perspective = true; // Toggle for perspective vs orthographic

    // Timing
    float gDeltaTime = 0.0f; // time between current frame and last frame
    float gLastFrame = 0.0f;
    float fov = 45.0f; // Field of view for perspective projection
}

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void UCreateMesh(GLMesh& mesh);
void UDestroyMesh(GLMesh& mesh);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);


/* Vertex Shader Source Code*/
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;
    layout(location = 1) in vec3 normal; // Assuming normal vectors are provided here
    layout(location = 2) in vec2 textureCoordinate;

    out vec2 vertexTextureCoordinate;
    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() 
    {
        FragPos = vec3(model * vec4(position, 1.0f));
        Normal = mat3(transpose(inverse(model))) * normal; // Transform normals
        vertexTextureCoordinate = textureCoordinate;
        gl_Position = projection * view * model * vec4(position, 1.0f);
    }
);


/* Fragment Shader Source Code */
const GLchar* fragmentShaderSource = GLSL(440,

    in vec2 vertexTextureCoordinate;
    in vec3 FragPos;
    in vec3 Normal;

    out vec4 fragmentColor;

    uniform sampler2D uTexture;
    uniform vec3 keyLightPos;
    uniform vec3 fillLightPos;
    uniform vec3 viewPos; // Make sure you actually use or remove this if it's not needed
    uniform vec3 keyLightColor;
    uniform vec3 fillLightColor;
    uniform float keyLightIntensity;
    uniform float fillLightIntensity;

    struct Spotlight {
        vec3 position;
        vec3 direction;
        vec3 color;
        float intensity;
        float cutOff;
        float outerCutOff;
        float constant;
        float linear;
        float quadratic;
    };

    uniform Spotlight spotlight;

    void main() {
        vec3 objectColor = texture(uTexture, vertexTextureCoordinate).rgb; // Use texture color

        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * (keyLightColor + fillLightColor);

        // Normals
        vec3 norm = normalize(Normal);

        // Key Light Calculations
        vec3 keyLightDir = normalize(keyLightPos - FragPos);
        float keyDiff = max(dot(norm, keyLightDir), 0.0);
        vec3 keyDiffuse = keyDiff * keyLightColor * keyLightIntensity;

        // Fill Light Calculations
        vec3 fillLightDir = normalize(fillLightPos - FragPos);
        float fillDiff = max(dot(norm, fillLightDir), 0.0);
        vec3 fillDiffuse = fillDiff * fillLightColor * fillLightIntensity;

        // Spotlight calculation
        vec3 lightDir = normalize(spotlight.position - FragPos);
        float theta = dot(lightDir, normalize(-spotlight.direction));
        float epsilon = spotlight.cutOff - spotlight.outerCutOff;
        float intensity = clamp((theta - spotlight.outerCutOff) / epsilon, 0.0, 1.0);
        float distance = length(spotlight.position - FragPos);
        float attenuation = 1.0 / (spotlight.constant + spotlight.linear * distance + spotlight.quadratic * (distance * distance));
        vec3 spotlightEffect = attenuation * intensity * spotlight.color * spotlight.intensity;

        // Combine the lighting components
        vec3 result = (ambient + keyDiffuse + fillDiffuse + spotlightEffect) * objectColor;
        fragmentColor = vec4(result, 1.0); // Set the final color
    }
);


// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}

// Main function
int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    // Creates the mesh
    UCreateMesh(gMesh); 

    // Creates the shader program
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
        return EXIT_FAILURE;

    // Loads glass texture
    const char* glassTexFilename = "../../resources/textures/glass.png";
    if (!UCreateTexture(glassTexFilename, gHemTor))
    {
        cout << "Failed to load texture " << glassTexFilename << endl;
        return EXIT_FAILURE;
    }

    // Loads gray texture
    const char* grayTexFilename = "../../resources/textures/gray.png";
    if (!UCreateTexture(grayTexFilename, gPlane))
    {
        cout << "Failed to load texture " << grayTexFilename << endl;
        return EXIT_FAILURE;
    }

    // Loads wood texture
    const char* woodTexFilename = "../../resources/textures/wood.png";
    if (!UCreateTexture(woodTexFilename, gRollPin))
    {
        cout << "Failed to load texture " << glassTexFilename << endl;
        return EXIT_FAILURE;
    }

    // Tells opengl for each sampler to which texture unit it belongs to 
    glUseProgram(gProgramId);
    glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

    // Sets the background color of the window to black (it will be implicitly used by glClear)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Enables blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render loop
    while (!glfwWindowShouldClose(gWindow))
    {
        // Per-frame timing
        float currentFrame = glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        // Input
        UProcessInput(gWindow);

        // Render this frame
        URender();

        glfwPollEvents();
    }

    // Releases mesh data
    UDestroyMesh(gMesh);

    // Releases texture
    UDestroyTexture(gTextureId);

    // Releases shader program
    UDestroyShaderProgram(gProgramId);

    // Terminates the program successfully
    exit(EXIT_SUCCESS);
}


// Initializes GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // GLFW: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: window creation
    // ---------------------
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);

    // Tells GLFW to capture our mouse
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW: initialize
    // ----------------
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    // Displays GPU OpenGL version
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}


// Process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window)
{
    float cameraSpeed = 2.5f * gDeltaTime; // Adjust cameraSpeed based on gDeltaTime for consistent speed regardless of frame rate
    static float movementSpeedFactor = 1.0f; // Default movement speed factor

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        gCamera.ProcessKeyboard(UP, gDeltaTime * movementSpeedFactor);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        gCamera.ProcessKeyboard(DOWN, gDeltaTime * movementSpeedFactor);

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        perspective = !perspective; // Toggle perspective
}


// Glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


// Glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos; // reversed since y-coordinates go from bottom to top

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}


// Glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.ProcessMouseScroll(yoffset);
}

// Glfw: handles mouse button events
// --------------------------------
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS)
            cout << "Middle mouse button pressed" << endl;
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}


// Renders the frame
void URender()
{
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind your VAO and shader program 
    glBindVertexArray(gMesh.vao);
    glUseProgram(gProgramId);

    // Setup lighting information
    glm::vec3 keyLightPos = glm::vec3(10.0f, 0.0f, 0.0f); // Adjusted position
    glm::vec3 keyLightColor = glm::vec3(1.0f, 1.0f, 1.0f); // Bright white
    float keyLightIntensity = 1.0f; // 100% intensity

    glUniform3f(glGetUniformLocation(gProgramId, "keyLightPos"), keyLightPos.x, keyLightPos.y, keyLightPos.z);
    glUniform3f(glGetUniformLocation(gProgramId, "keyLightColor"), keyLightColor.x, keyLightColor.y, keyLightColor.z);
    glUniform1f(glGetUniformLocation(gProgramId, "keyLightIntensity"), keyLightIntensity);

    glm::vec3 fillLightPos = glm::vec3(-5.0f, 10.0f, 10.0f); // Adjusted position
    glm::vec3 fillLightColor = glm::vec3(1.0f, 1.0f, 1.0f); // white
    float fillLightIntensity = 0.0f; // 10% intensity

    glUniform3f(glGetUniformLocation(gProgramId, "fillLightPos"), fillLightPos.x, fillLightPos.y, fillLightPos.z);
    glUniform3f(glGetUniformLocation(gProgramId, "fillLightColor"), fillLightColor.x, fillLightColor.y, fillLightColor.z);
    glUniform1f(glGetUniformLocation(gProgramId, "fillLightIntensity"), fillLightIntensity);

    // Spotlight properties
    glm::vec3 spotlightPosition = glm::vec3(1.0f, 5.0f, 6.0f); 
    glm::vec3 spotlightDirection = glm::vec3(0.0f, -1.0f, -1.0f); 
    glm::vec3 spotlightColor = glm::vec3(0.5f, 0.7f, 1.0f); // Light blue
    float spotlightIntensity = 1.0f;
    float spotlightCutOff = glm::cos(glm::radians(12.5f));
    float spotlightOuterCutOff = glm::cos(glm::radians(15.0f));
    float spotlightConstant = 1.0f;
    float spotlightLinear = 0.09f;
    float spotlightQuadratic = 0.032f;

    // Set spotlight uniforms
    glUniform3f(glGetUniformLocation(gProgramId, "spotlight.position"), spotlightPosition.x, spotlightPosition.y, spotlightPosition.z);
    glUniform3f(glGetUniformLocation(gProgramId, "spotlight.direction"), spotlightDirection.x, spotlightDirection.y, spotlightDirection.z);
    glUniform3f(glGetUniformLocation(gProgramId, "spotlight.color"), spotlightColor.x, spotlightColor.y, spotlightColor.z);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.intensity"), spotlightIntensity);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.cutOff"), spotlightCutOff);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.outerCutOff"), spotlightOuterCutOff);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.constant"), spotlightConstant);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.linear"), spotlightLinear);
    glUniform1f(glGetUniformLocation(gProgramId, "spotlight.quadratic"), spotlightQuadratic);

    // Scales the object uniformly
    glm::mat4 scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));

    // Rotates shape by 90 degrees around the x-axis
    glm::mat4 rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Places object at the origin
    glm::mat4 translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));

    // Model matrix: transformations are applied right-to-left order
    glm::mat4 model = translation * rotation * scale;

    glm::mat4 view = gCamera.GetViewMatrix();

    // Creates a perspective projection
    glm::mat4 projection;
    if (perspective) // Ensure 'perspective' variable is correctly defined or passed
    {
        projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);
    }
    else
    {
        projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    }

    glUniformMatrix4fv(glGetUniformLocation(gProgramId, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(gProgramId, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(gProgramId, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Draw coordinates
    GLuint hemisphereIndexOffset = 0;
    GLuint hemisphereIndexCount = 59400;
    GLuint torusIndexOffset = 59400;
    GLuint torusIndexCount = 12000;
    GLuint planeIndexOffset = 71400;
    GLuint planeIndexCount = 8;
    GLuint cylinderIndexOffset = 71406;
    GLuint cylinderIndexCount = 2400;
    GLuint handleIndexCount = 73806;
    GLuint firstHandleIndexOffset = 2400;
    GLuint cylinderTopCapIndexCount = 60;
    GLuint cylinderTopCapIndexOffset = 73806;
    GLuint cylinderBottomCapIndexCount = 60;
    GLuint cylinderBottomCapIndexOffset = 73868;
    GLuint innerCylinderTopCapIndexOffset = cylinderBottomCapIndexOffset + cylinderBottomCapIndexCount; 
    GLuint innerCylinderTopCapIndexCount = 60;
    GLuint innerCylinderBottomCapIndexOffset = innerCylinderTopCapIndexOffset + innerCylinderTopCapIndexCount; 
    GLuint innerCylinderBottomCapIndexCount = 60;
    GLuint eggsIndexOffset = 73986;
    GLuint eggsIndexCount = 2400;
    GLuint secondHandleIndexOffset = firstHandleIndexOffset + handleIndexCount;

    glBindTexture(GL_TEXTURE_2D, gHemTor);
    glDrawElements(GL_TRIANGLES, hemisphereIndexCount, GL_UNSIGNED_INT, (void*)(hemisphereIndexOffset * sizeof(GLuint)));
    glDrawElements(GL_TRIANGLES, torusIndexCount, GL_UNSIGNED_INT, (void*)(torusIndexOffset * sizeof(GLuint)));

    glActiveTexture(GL_TEXTURE0); // Activate the texture unit
    glBindTexture(GL_TEXTURE_2D, gPlane);
    glDrawElements(GL_TRIANGLES, planeIndexCount, GL_UNSIGNED_INT, (void*)(planeIndexOffset * sizeof(GLuint)));

    // Draw the cylinder body
    glActiveTexture(GL_TEXTURE0); // Activate the texture unit
    glBindTexture(GL_TEXTURE_2D, gRollPin);
    glDrawElements(GL_TRIANGLES, cylinderIndexCount, GL_UNSIGNED_INT, (void*)(cylinderIndexOffset * sizeof(GLuint)));

    // Draw the first handle
    glDrawElements(GL_TRIANGLES, handleIndexCount, GL_UNSIGNED_INT, (void*)(firstHandleIndexOffset * sizeof(GLuint)));

    // Draw the second handle
    glDrawElements(GL_TRIANGLES, handleIndexCount, GL_UNSIGNED_INT, (void*)(secondHandleIndexOffset * sizeof(GLuint)));

    // Draw the cylinder top cap
    glDrawElements(GL_TRIANGLES, cylinderTopCapIndexCount, GL_UNSIGNED_INT, (void*)(cylinderTopCapIndexOffset * sizeof(GLuint)));

    // Draw the cylinder bottom cap
    glDrawElements(GL_TRIANGLES, cylinderBottomCapIndexCount, GL_UNSIGNED_INT, (void*)(cylinderBottomCapIndexOffset * sizeof(GLuint)));

    // Draw the inner cylinder caps
    glDrawElements(GL_TRIANGLES, innerCylinderTopCapIndexCount, GL_UNSIGNED_INT, (void*)(innerCylinderTopCapIndexOffset * sizeof(GLuint)));
    glDrawElements(GL_TRIANGLES, innerCylinderBottomCapIndexCount, GL_UNSIGNED_INT, (void*)(innerCylinderBottomCapIndexOffset * sizeof(GLuint)));

    // Draw the eggs
    glDrawElements(GL_TRIANGLES, eggsIndexCount, GL_UNSIGNED_INT, (void*)(eggsIndexOffset * sizeof(GLuint)));
    
    glBindVertexArray(0);
    glfwSwapBuffers(gWindow);
}


// Implements the UCreateMesh function
void UCreateMesh(GLMesh& mesh) {
    // hemisphere parameters
    const unsigned int stacks = 100; // Hemisphere
    const unsigned int sectors = 100; // Hemisphere
    const float radius = 1.0f; // Hemisphere
    const float PI = 3.14159265358979323846f;

    // Torus parameters
    const float torusInnerRadius = 0.1f;
    const float torusOuterRadius = 1.0f;
    const unsigned int torusStacks = 20;
    const unsigned int torusSectors = 100;

    // Cylinder parameters
    const unsigned int cylinderStacks = 20; 
    const unsigned int cylinderSectors = 20; 
    const float cylinderHeight = 2.0f; // Main cylinder length
    const float cylinderRadius = 0.2f; // Main cylinder radius
    float cylinderTranslationX = 2.0f; 
    float cylinderTranslationZ = 0.8f; 

    // Inner cylinder parameters
    float innerCylinderRadius = 0.05f; // Adjust the radius to be thinner
    float innerCylinderHeight = 3.0f; 

    // Egg parameters
    const unsigned int eggStacks = 20;
    const unsigned int eggSectors = 20;
    const float eggRadius = 0.2f; // Base radius for the egg
    const float eggScaleX = 0.75f; // Scale on X to make it egg-shaped
    const float eggScaleY = 1.2f; // Scale on Y to make it egg-shaped
    const float eggScaleZ = 0.75f; // Scale on Z to make it egg-shaped

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Adjustment for hemisphere to attach to the left side of the cylinder
    float hemisphereTranslationX = cylinderTranslationX; // Aligned with the cylinder's center
    float hemisphereTranslationY = -cylinderHeight / 2.0f - radius; // Left side, considering hemisphere radius
    float hemisphereTranslationZ = cylinderTranslationZ; // Same Z as the cylinder to ensure it's on the plane

    // Hemispere vertices
    for (unsigned int i = 0; i <= stacks; ++i) {
        float stackAngle = PI / 2 * i / stacks;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (unsigned int j = 0; j <= sectors; ++j) {
            float sectorAngle = 2 * PI * j / sectors;

            float x = xy * cosf(sectorAngle) + hemisphereTranslationX - 0.9;
            float y = xy * sinf(sectorAngle) + hemisphereTranslationY + 1.5;
            float zAdjusted = z + hemisphereTranslationZ - 0.8; 

            // Push back position and texture coordinates
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(zAdjusted);
            float s = (float)j / sectors;
            float t = (float)i / stacks;
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    // Hemisphere indices
    for (unsigned int i = 0; i < stacks; ++i) {
        unsigned int k1 = i * (sectors + 1); // beginning of current stack
        unsigned int k2 = k1 + sectors + 1; // beginning of next stack

        for (unsigned int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    unsigned int hemisphereVertexCount = (stacks + 1) * (sectors + 1);

    // Calculate torus vertical adjustment
    float torusVerticalAdjustment = radius + torusInnerRadius; 

    // Torus vertices to sit on top of the hemisphere
    for (unsigned int i = 0; i <= torusStacks; ++i) {
        float stackAngle = 2 * PI * i / torusStacks;
        for (unsigned int j = 0; j <= torusSectors; ++j) {
            float sectorAngle = 2 * PI * j / torusSectors;

            float x = (torusOuterRadius + torusInnerRadius * cosf(sectorAngle)) * cosf(stackAngle) + 1.1;
            float y = (torusOuterRadius + torusInnerRadius * cosf(sectorAngle)) * sinf(stackAngle) + torusVerticalAdjustment + hemisphereTranslationY + 0.4; // Adjust for vertical positioning on top of the hemisphere
            float z = torusInnerRadius * sinf(sectorAngle);

            // Push back position and texture coordinates with appropriate adjustments
            vertices.push_back(x);
            vertices.push_back(y); // Y is adjusted to place the torus on top of the hemisphere
            vertices.push_back(z);
            vertices.push_back((float)j / torusSectors);
            vertices.push_back((float)i / torusStacks);
        }
    }

    // Torus indices
    for (unsigned int i = 0; i < torusStacks; ++i) {
        unsigned int k1 = hemisphereVertexCount + i * (torusSectors + 1);
        unsigned int k2 = k1 + torusSectors + 1;

        for (unsigned int j = 0; j < torusSectors; ++j, ++k1, ++k2) {
            indices.push_back(k1);
            indices.push_back(k2);
            indices.push_back(k1 + 1);

            indices.push_back(k1 + 1);
            indices.push_back(k2);
            indices.push_back(k2 + 1);
        }
    }

    // Plane vertices and texture coordinates
    const float planeSize = 5.0f;
    const float planeHeight = 1.0f; // Height of the plane
    unsigned int planeVertexStartIndex = vertices.size() / 5; // Start index for plane vertices

    // Bottom left
    vertices.push_back(-planeSize); vertices.push_back(-planeSize); vertices.push_back(planeHeight);
    vertices.push_back(0.0f); vertices.push_back(0.0f);

    // Bottom right
    vertices.push_back(planeSize); vertices.push_back(-planeSize); vertices.push_back(planeHeight);
    vertices.push_back(1.0f); vertices.push_back(0.0f);

    // Top right
    vertices.push_back(planeSize); vertices.push_back(planeSize); vertices.push_back(planeHeight);
    vertices.push_back(1.0f); vertices.push_back(1.0f);

    // Top left
    vertices.push_back(-planeSize); vertices.push_back(planeSize); vertices.push_back(planeHeight);
    vertices.push_back(0.0f); vertices.push_back(1.0f);

    // Plane indices
    indices.push_back(planeVertexStartIndex);
    indices.push_back(planeVertexStartIndex + 1);
    indices.push_back(planeVertexStartIndex + 2);

    indices.push_back(planeVertexStartIndex);
    indices.push_back(planeVertexStartIndex + 2);
    indices.push_back(planeVertexStartIndex + 3);

    unsigned int cylinderVertexStartIndex = vertices.size() / 5;

    // Cylinder vertices (Rolling Pin Body)
    for (unsigned int i = 0; i <= cylinderStacks; ++i) {
        float y = (float)i / cylinderStacks * cylinderHeight - (cylinderHeight / 2.0f); // Centered on the height
        for (unsigned int j = 0; j <= cylinderSectors; ++j) {
            float sectorAngle = 2 * PI * j / cylinderSectors;
            float x = cylinderRadius * cosf(sectorAngle) + cylinderTranslationX; // Translate horizontally
            float z = cylinderRadius * sinf(sectorAngle) + cylinderTranslationZ; // Adjust height to sit on plane

            // Texture coordinates
            float s = (float)j / cylinderSectors; // Horizontal wrap of texture
            float t = (float)i / cylinderStacks; // Vertical stretch of texture

            // Push position and texture coordinates
            vertices.push_back(x); // Adjusted for horizontal position
            vertices.push_back(y); // y is used for length along the "rolling pin"
            vertices.push_back(z);
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    // Cylinder indices
    for (unsigned int i = 0; i < cylinderStacks; ++i) {
        unsigned int k1 = cylinderVertexStartIndex + i * (cylinderSectors + 1); // beginning of current stack
        unsigned int k2 = k1 + cylinderSectors + 1; // beginning of next stack

        for (unsigned int j = 0; j < cylinderSectors; ++j, ++k1, ++k2) {
            indices.push_back(k1);
            indices.push_back(k2);
            indices.push_back(k1 + 1);

            indices.push_back(k1 + 1);
            indices.push_back(k2);
            indices.push_back(k2 + 1);
        }
    }

    unsigned int innerCylinderVertexStartIndex = vertices.size() / 5;

    // Vertices for the inner cylinder
    for (unsigned int i = 0; i <= cylinderStacks; ++i) {
        float y = (float)i / cylinderStacks * innerCylinderHeight - (innerCylinderHeight / 2.0f); // Centered on the height
        for (unsigned int j = 0; j <= cylinderSectors; ++j) {
            float sectorAngle = 2 * PI * j / cylinderSectors;
            float x = innerCylinderRadius * cosf(sectorAngle) + cylinderTranslationX; 
            float z = innerCylinderRadius * sinf(sectorAngle) + cylinderTranslationZ; 

            // Texture coordinates
            float s = (float)j / cylinderSectors; // Horizontal wrap of texture
            float t = (float)i / cylinderStacks; // Vertical stretch of texture

            // Push position and texture coordinates for the thinner cylinder
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    // Indices for the inner cylinder
    for (unsigned int i = 0; i < cylinderStacks; ++i) {
        unsigned int k1 = innerCylinderVertexStartIndex + i * (cylinderSectors + 1);
        unsigned int k2 = k1 + cylinderSectors + 1;

        for (unsigned int j = 0; j < cylinderSectors; ++j, ++k1, ++k2) {
            indices.push_back(k1);
            indices.push_back(k2);
            indices.push_back(k1 + 1);

            indices.push_back(k1 + 1);
            indices.push_back(k2);
            indices.push_back(k2 + 1);
        }
    }

    // Calculate the positions based on the right end of the cylinder
    float cylinderEndX = cylinderTranslationX + cylinderRadius - 0.1f;

    const float eggSeparation = 0.1f;

    // Position of the first egg (lying horizontally)
    const float egg1PositionX = cylinderEndX + eggRadius * eggScaleX + eggSeparation + 0.0;
    const float egg1PositionY = 0.5f;
    const float egg1PositionZ = cylinderTranslationZ + 0.05; // Adjusted Z position

    // Position of the second egg (vertically standing, next to the first egg)
    const float egg2PositionX = egg1PositionX + 1.3 * eggRadius * eggScaleX + eggSeparation - 0.3;
    const float egg2PositionY = 0.12f;
    const float egg2PositionZ = cylinderTranslationZ - 0.05f;

    float cosAngleX = cosf(PI / 2); // Cosine of rotation angle for 90 degrees
    float sinAngleX = sinf(PI / 2); // Sine of rotation angle for 90 degrees

    for (int egg = 0; egg < 2; ++egg) {
        float eggPositionX = (egg == 1) ? egg2PositionX : egg1PositionX;
        float eggPositionY = (egg == 1) ? egg2PositionY : egg1PositionY;
        // Assign the correct Z position based on the egg index
        float eggPositionZ = (egg == 1) ? egg2PositionZ : egg1PositionZ;

        unsigned int eggVertexStartIndex = vertices.size() / 5; // 5 components per vertex (x, y, z, s, t)
        for (unsigned int i = 0; i <= eggStacks; ++i) {
            float stackAngle = PI * i / eggStacks;
            for (unsigned int j = 0; j <= eggSectors; ++j) {
                float sectorAngle = 2 * PI * j / eggSectors;
                float x = eggRadius * cosf(sectorAngle) * sinf(stackAngle) * eggScaleX;
                float y = eggRadius * cosf(stackAngle) * eggScaleY;
                float z = eggRadius * sinf(sectorAngle) * sinf(stackAngle) * eggScaleZ;
                if (egg == 1) {
                    float tempY = y;

                    // Rotate around the X-axis to lay the egg down
                    y = cosAngleX * tempY - sinAngleX * z;
                    z = sinAngleX * tempY + cosAngleX * z;
                }

                // Translate vertices to their final positions
                x += eggPositionX;
                y += eggPositionY;
                z += eggPositionZ;

                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                float s = (float)j / eggSectors;
                float t = (float)i / eggStacks;
                vertices.push_back(s);
                vertices.push_back(t);
            }
        }

        // Indices generation
        for (unsigned int i = 0; i < eggStacks; ++i) {
            for (unsigned int j = 0; j < eggSectors; ++j) {
                unsigned int first = eggVertexStartIndex + i * (eggSectors + 1) + j;
                unsigned int second = first + eggSectors + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(first + 1);
                indices.push_back(second);
                indices.push_back(second + 1);
            }
        }
    }

    // Top cap for the OUTER cylinder
    float topYOuter = cylinderHeight / 2.0f; // Top cap y coordinate for the outer cylinder
    unsigned int topCenterIndexOuter = vertices.size() / 5; // Index of the top center vertex for the outer cylinder

    // Center vertex for the OUTER cylinder top cap
    vertices.push_back(cylinderTranslationX); vertices.push_back(topYOuter); vertices.push_back(cylinderTranslationZ);
    vertices.push_back(0.5f); vertices.push_back(0.5f); // Texture coordinates for the center

    // Outer cylinder top cap vertices and indices
    for (unsigned int j = 1; j <= cylinderSectors; ++j) {
        float sectorAngle = 2 * PI * j / cylinderSectors;
        float x = cylinderRadius * cosf(sectorAngle) + cylinderTranslationX;
        float z = cylinderRadius * sinf(sectorAngle) + cylinderTranslationZ;

        vertices.push_back(x); vertices.push_back(topYOuter); vertices.push_back(z);
        vertices.push_back((cosf(sectorAngle) + 1.0f) * 0.5f); vertices.push_back((sinf(sectorAngle) + 1.0f) * 0.5f);

        if (j < cylinderSectors) { // Add triangles for top cap
            indices.push_back(topCenterIndexOuter);
            indices.push_back(topCenterIndexOuter + j);
            indices.push_back(topCenterIndexOuter + j + 1);
        }
    }
    indices.push_back(topCenterIndexOuter); 
    indices.push_back(topCenterIndexOuter + cylinderSectors);
    indices.push_back(topCenterIndexOuter + 1);

    // Bottom cap for the OUTER cylinder
    float bottomYOuter = -cylinderHeight / 2.0f; // Bottom cap y coordinate for the outer cylinder
    unsigned int bottomCenterIndexOuter = vertices.size() / 5; // Index of the bottom center vertex for the outer cylinder

    // Center vertex for the OUTER cylinder bottom cap
    vertices.push_back(cylinderTranslationX); vertices.push_back(bottomYOuter); vertices.push_back(cylinderTranslationZ);
    vertices.push_back(0.5f); vertices.push_back(0.5f); // Texture coordinates for the center

    // Outer cylinder bottom cap vertices and indices
    for (unsigned int j = 1; j <= cylinderSectors; ++j) {
        float sectorAngle = 2 * PI * j / cylinderSectors;
        float x = cylinderRadius * cosf(sectorAngle) + cylinderTranslationX;
        float z = cylinderRadius * sinf(sectorAngle) + cylinderTranslationZ;

        vertices.push_back(x); vertices.push_back(bottomYOuter); vertices.push_back(z);
        vertices.push_back((cosf(sectorAngle) + 1.0f) * 0.5f); vertices.push_back((sinf(sectorAngle) + 1.0f) * 0.5f);

        if (j < cylinderSectors) { // Add triangles for bottom cap, reversing order
            indices.push_back(bottomCenterIndexOuter);
            indices.push_back(bottomCenterIndexOuter + j + 1);
            indices.push_back(bottomCenterIndexOuter + j);
        }
    }
    indices.push_back(bottomCenterIndexOuter); 
    indices.push_back(bottomCenterIndexOuter + 1);
    indices.push_back(bottomCenterIndexOuter + cylinderSectors);

    // Top cap for the INNER cylinder
    float topYInner = innerCylinderHeight / 2.0f; // Top cap y coordinate for the inner cylinder
    unsigned int topCenterIndexInner = vertices.size() / 5; // Index of the top center vertex for the inner cylinder

    // Center vertex for the INNER cylinder top cap
    vertices.push_back(cylinderTranslationX); vertices.push_back(topYInner); vertices.push_back(cylinderTranslationZ);
    vertices.push_back(0.5f); vertices.push_back(0.5f); // Texture coordinates for the center

    // Inner cylinder top cap vertices and indices
    for (unsigned int j = 1; j <= cylinderSectors; ++j) {
        float sectorAngle = 2 * PI * j / cylinderSectors;
        float x = innerCylinderRadius * cosf(sectorAngle) + cylinderTranslationX;
        float z = innerCylinderRadius * sinf(sectorAngle) + cylinderTranslationZ;

        vertices.push_back(x); vertices.push_back(topYInner); vertices.push_back(z);
        vertices.push_back((cosf(sectorAngle) + 1.0f) * 0.5f); vertices.push_back((sinf(sectorAngle) + 1.0f) * 0.5f);

        if (j < cylinderSectors) { 
            indices.push_back(topCenterIndexInner);
            indices.push_back(topCenterIndexInner + j);
            indices.push_back(topCenterIndexInner + j + 1);
        }
    }
    indices.push_back(topCenterIndexInner); 
    indices.push_back(topCenterIndexInner + cylinderSectors);
    indices.push_back(topCenterIndexInner + 1);

    // Bottom cap for the INNER cylinder
    float bottomYInner = -innerCylinderHeight / 2.0f; // Bottom cap y coordinate for the inner cylinder
    unsigned int bottomCenterIndexInner = vertices.size() / 5; // Index of the bottom center vertex for the inner cylinder

    // Center vertex for the INNER cylinder bottom cap
    vertices.push_back(cylinderTranslationX); vertices.push_back(bottomYInner); vertices.push_back(cylinderTranslationZ);
    vertices.push_back(0.5f); vertices.push_back(0.5f); // Texture coordinates for the center

    // Inner cylinder bottom cap vertices and indices
    for (unsigned int j = 1; j <= cylinderSectors; ++j) {
        float sectorAngle = 2 * PI * j / cylinderSectors;
        float x = innerCylinderRadius * cosf(sectorAngle) + cylinderTranslationX;
        float z = innerCylinderRadius * sinf(sectorAngle) + cylinderTranslationZ;

        vertices.push_back(x); vertices.push_back(bottomYInner); vertices.push_back(z);
        vertices.push_back((cosf(sectorAngle) + 1.0f) * 0.5f); vertices.push_back((sinf(sectorAngle) + 1.0f) * 0.5f);

        if (j < cylinderSectors) { // Add triangles for bottom cap, reversing order
            indices.push_back(bottomCenterIndexInner);
            indices.push_back(bottomCenterIndexInner + j + 1);
            indices.push_back(bottomCenterIndexInner + j);
        }
    }
    indices.push_back(bottomCenterIndexInner); 
    indices.push_back(bottomCenterIndexInner + 1);
    indices.push_back(bottomCenterIndexInner + cylinderSectors);

    // Generate VAO, VBO, and EBO
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind the VAO
    glBindVertexArray(0);

    // Save the total count of indices for rendering
    mesh.nIndices = indices.size();
}


void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
}


/*Generates and loads the texture*/
bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // Sets the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Sets texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

        return true;
    }

    // Error loading the image
    return false;
}


void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}


// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    // Compilation and linkage error reporting
    int success = 0;
    char infoLog[512];

    // Create a Shader program object.
    programId = glCreateProgram();

    // Create the vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrive the shader source
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile the vertex shader, and print compilation errors (if any)
    glCompileShader(vertexShaderId); // compile the vertex shader
    // check for shader compile errors
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glCompileShader(fragmentShaderId); // compile the fragment shader
    // check for shader compile errors
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);   // links the shader program
    // check for linking errors
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glUseProgram(programId);    // Uses the shader program

    return true;
}


void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}
