# OpenGL 3D Billboard Text Renderer

A modern OpenGL application that displays **3D cubes with dynamically rendered text labels**.  
Each label is rendered as a **billboard** (always facing the camera), using **geometry shaders** and a **font atlas**.  
Objects closest to the camera are automatically highlighted for better visibility.

---

## Features

- 3D cube rendering with perspective camera  
-  Free camera movement (yaw, pitch, WASD)  
-  Text rendering using geometry shaders (billboards)  
-  Font atlas support (ASCII 32–127)  
-  Dynamic color highlighting for nearest object  
-  Lightweight C++17 + OpenGL 4.6 Core  
-  Efficient batching and GPU instancing  

---

##  Technologies Used

| Component | Description |
|------------|--------------|
| **C++17** | Core application language |
| **OpenGL 3.3** | Graphics rendering API |
| **GLFW** | Window, input & context management |
| **GLAD** | OpenGL function loader |
| **GLM** | Math library for matrices & vectors |
| **stb_image.h** | Texture and font atlas loading |

---

## How It Works

###  Geometry Shader Billboards
Each text character is passed as a single vertex point.  
The **geometry shader** expands it into a quad facing the camera using the view matrix’s right and up vectors.

```glsl
vec3 right = vec3(view[0][0], view[1][0], view[2][0]) * size;
vec3 up    = vec3(view[0][1], view[1][1], view[2][1]) * size;

