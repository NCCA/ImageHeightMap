#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Transformation.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>
#include <iostream>
NGLScene::NGLScene(std::string _fname)
{
  setTitle("Qt5 Simple NGL Demo");
  m_imageName = _fname;
}

NGLScene::~NGLScene()
{
  std::cout << "Shutting down NGL, removing VAO's and Shaders\n";
}

void NGLScene::genGridPoints(ngl::Real _width, ngl::Real _depth)
{
  // load our image and get size
  QImage image(m_imageName.c_str());
  int imageWidth = image.size().width() - 1;
  int imageHeight = image.size().height() - 1;
  std::cout << "image size " << imageWidth << " " << imageHeight << "\n";

  // calculate the deltas for the x,z values of our point
  float wStep = static_cast<float>(_width) / imageWidth;
  float dStep = static_cast<float>(_depth) / imageHeight;
  // now we assume that the grid is centered at 0,0,0 so we make
  // it flow from -w/2 -d/2
  float xPos = -(_width / 2.0f);
  float zPos = -(_depth / 2.0f);
  // now loop from top left to bottom right and generate points
  std::vector<ngl::Vec3> gridPoints;

  for (int z = 0; z <= imageHeight; ++z)
  {
    for (int x = 0; x <= imageWidth; ++x)
    {
      // grab the colour and use for the Y (height) only use the red channel
      QColor c(image.pixel(x, z));
      gridPoints.push_back(ngl::Vec3(xPos, c.redF() * 4.0f, zPos));
      // now store the colour as well
      gridPoints.push_back(ngl::Vec3(c.redF(), c.greenF(), c.blueF()));
      // calculate the new position
      xPos += wStep;
    }
    // now increment to next z row
    zPos += dStep;
    // we need to re-set the xpos for new row
    xPos = -(_width / 2.0f);
  }

  std::vector<GLuint> indices;
  // some unique index value to indicate we have finished with a row and
  // want to draw a new one
  GLuint restartFlag = imageWidth * imageHeight + 9999u;

  for (int z = 0; z < imageHeight; ++z)
  {
    for (int x = 0; x < imageWidth; ++x)
    {
      // Vertex in actual row
      indices.push_back(z * (imageWidth + 1) + x);
      // Vertex row below
      indices.push_back((z + 1) * (imageWidth + 1) + x);
    }
    // now we have a row of tri strips signal a re-start
    indices.push_back(restartFlag);
  }

  // we could use an ngl::VertexArrayObject but in this case this will show how to
  // create our own as a demo / reminder
  // so first create a vertex array
  glGenVertexArrays(1, &m_vaoID);
  glBindVertexArray(m_vaoID);

  // now a VBO for the grid point data
  GLuint vboID;
  glGenBuffers(1, &vboID);
  glBindBuffer(GL_ARRAY_BUFFER, vboID);
  glBufferData(GL_ARRAY_BUFFER, gridPoints.size() * sizeof(ngl::Vec3), &gridPoints[0].m_x, GL_STATIC_DRAW);

  // and one for the index values
  GLuint iboID;
  glGenBuffers(1, &iboID);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboID);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

  // setup our attribute pointers, we are using 0 for the verts (note the step is going to
  // be 2*Vec3
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ngl::Vec3) * 2, 0);
  // this once is the colour pointer and we need to offset by 3 floats
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ngl::Vec3) * 2, ((float *)NULL + (3)));
  // enable the pointers
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  // now we tell gl to restart the primitives when restartFlag is encountered
  // don't forget we may need to disable this for other prims so may have to set the
  // state when required
  glEnable(GL_PRIMITIVE_RESTART);
  glPrimitiveRestartIndex(restartFlag);
  glBindVertexArray(0);
  // store the number of indices for re-drawing later
  m_vertSize = indices.size();
}

void NGLScene::initializeGL()
{
  // we must call this first before any other GL commands to load and link the
  // gl commands from the lib, if this is not done program will crash
  ngl::NGLInit::initialize();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f); // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  // enable multisampling for smoother drawing
  glEnable(GL_MULTISAMPLE);

  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0, 10, 54);
  ngl::Vec3 to(0, 0, 0);
  ngl::Vec3 up(0, 1, 0);

  m_view = ngl::lookAt(from, to, up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_project = ngl::perspective(45, 720.0f / 576.0f, 0.001f, 150);

  ngl::ShaderLib::createShaderProgram("ColourShader");

  ngl::ShaderLib::attachShader("ColourVertex", ngl::ShaderType::VERTEX);
  ngl::ShaderLib::attachShader("ColourFragment", ngl::ShaderType::FRAGMENT);
  ngl::ShaderLib::loadShaderSource("ColourVertex", "shaders/ColourVert.glsl");
  ngl::ShaderLib::loadShaderSource("ColourFragment", "shaders/ColourFrag.glsl");

  ngl::ShaderLib::compileShader("ColourVertex");
  ngl::ShaderLib::compileShader("ColourFragment");
  ngl::ShaderLib::attachShaderToProgram("ColourShader", "ColourVertex");
  ngl::ShaderLib::attachShaderToProgram("ColourShader", "ColourFragment");

  ngl::ShaderLib::linkProgramObject("ColourShader");
  ngl::ShaderLib::autoRegisterUniforms("ColourShader");
  ngl::ShaderLib::use("ColourShader");

  genGridPoints(40, 40);
}

void NGLScene::paintGL()
{
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, m_win.width, m_win.height);
  // Rotation based on the mouse position for our global transform
  ngl::Mat4 rotX = ngl::Mat4::rotateX(m_win.spinXFace);
  ngl::Mat4 rotY = ngl::Mat4::rotateY(m_win.spinYFace);
  // multiply the rotations
  m_mouseGlobalTX = rotY * rotX;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;
  ngl::ShaderLib::use("ColourShader");

  ngl::Mat4 MVP;
  MVP = m_project * m_view * m_mouseGlobalTX;

  ngl::ShaderLib::setUniform("MVP", MVP);

  // bind our VAO
  glBindVertexArray(m_vaoID);
  // draw as triangle strips
  glDrawElements(GL_TRIANGLE_STRIP, m_vertSize, GL_UNSIGNED_INT, 0); // draw first object
}

void NGLScene::resizeGL(int _w, int _h)
{
  m_project = ngl::perspective(45.0f, static_cast<float>(_w) / _h, 0.05f, 350.0f);
  m_win.width = static_cast<int>(_w * devicePixelRatio());
  m_win.height = static_cast<int>(_h * devicePixelRatio());
}
//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quite
  case Qt::Key_Escape:
    QGuiApplication::exit(EXIT_SUCCESS);
    break;
  // turn on wirframe rendering
  case Qt::Key_W:
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    break;
  // turn off wire frame
  case Qt::Key_S:
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    break;
  // show full screen
  case Qt::Key_F:
    showFullScreen();
    break;
  // show windowed
  case Qt::Key_N:
    showNormal();
    break;
  default:
    break;
  }
  // finally update the GLWindow and re-draw
  // if (isExposed())
  update();
}
