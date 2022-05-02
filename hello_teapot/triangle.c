/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2012, OtherCrashOverride
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// A rotating cube rendered with OpenGL|ES. Three images used as textures on the cube faces.


#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define EXIT(msg) { fputs (msg, stderr); exit (EXIT_FAILURE); }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include "cube_texture_and_coords.h"
#include "models.h"
#include "triangle.h"
#include <pthread.h>

#include <signal.h>

#define PATH "./"

#define IMAGE_SIZE_WIDTH 1920
#define IMAGE_SIZE_HEIGHT 1080

#ifndef M_PI
   #define M_PI 3.141592654
#endif
  

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   GLuint tex;
// model rotation vector and direction
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
// current model rotation angles
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
// current distance from camera
   GLfloat distance;
   GLfloat distance_inc;
   MODEL_T model;
} CUBE_STATE_T;

static void init_ogl(CUBE_STATE_T *state);
static void init_model_proj(CUBE_STATE_T *state);
static void reset_model(CUBE_STATE_T *state);
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc);
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc);
static void redraw_scene(CUBE_STATE_T *state);
static void update_model(CUBE_STATE_T *state);
static void init_textures(CUBE_STATE_T *state);
static void exit_func(void);
static volatile int terminate;
static CUBE_STATE_T _state, *state=&_state;

static void* eglImage = 0;
static pthread_t thread1;

extern int thread_run;


/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(CUBE_STATE_T *state)
{
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_SAMPLES, 4,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   EGLConfig config;

   // Set background color and clear buffers
   glClearColor((0.3922f+7*0.5f)/8, (0.1176f+7*0.5f)/8, (0.5882f+7*0.5f)/8, 1.0f);

   // Enable back face culling.
   glEnable(GL_CULL_FACE);

   glEnable(GL_DEPTH_TEST);
   glClearDepthf(1.0);
   glDepthFunc(GL_LEQUAL);

   float noAmbient[] = {1.0f, 1.0f, 1.0f, 1.0f};
   glLightfv(GL_LIGHT0, GL_AMBIENT, noAmbient);
   glEnable(GL_LIGHT0);
   glEnable(GL_LIGHTING);
}

/***********************************************************
 * Name: init_model_proj
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the OpenGL|ES model to default values
 *
 * Returns: void
 *
 ***********************************************************/
static void init_model_proj(CUBE_STATE_T *state)
{
   float nearp = 0.1f;
   float farp = 500.0f;
   float hht;
   float hwd;

   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

   glViewport(0, 0, (GLsizei)state->screen_width, (GLsizei)state->screen_height);
      
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
   
   glEnableClientState( GL_VERTEX_ARRAY );

   reset_model(state);
}

/***********************************************************
 * Name: reset_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Resets the Model projection and rotation direction
 *
 * Returns: void
 *
 ***********************************************************/
static void reset_model(CUBE_STATE_T *state)
{
   // reset model position
   glMatrixMode(GL_MODELVIEW);

   // reset model rotation
   state->rot_angle_x = 45.f; state->rot_angle_y = 30.f; state->rot_angle_z = 0.f;
   state->rot_angle_x_inc = 0.5f; state->rot_angle_y_inc = 0.5f; state->rot_angle_z_inc = 0.f;
   state->distance = 1.2f*1.5f;
}

/***********************************************************
 * Name: update_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Updates model projection to current position/rotation
 *
 * Returns: void
 *
 ***********************************************************/
static void update_model(CUBE_STATE_T *state)
{
   // update position
   state->rot_angle_x = inc_and_wrap_angle(state->rot_angle_x, state->rot_angle_x_inc);
   state->rot_angle_y = inc_and_wrap_angle(state->rot_angle_y, state->rot_angle_y_inc);
   state->rot_angle_z = inc_and_wrap_angle(state->rot_angle_z, state->rot_angle_z_inc);
   state->distance    = inc_and_clip_distance(state->distance, state->distance_inc);

   glLoadIdentity();
   // move camera back to see the cube
   glTranslatef(0.f, 0.f, -state->distance);

   // Rotate model to new position
   glRotatef(state->rot_angle_x, 1.f, 0.f, 0.f);
   glRotatef(state->rot_angle_y, 0.f, 1.f, 0.f);
   glRotatef(state->rot_angle_z, 0.f, 0.f, 1.f);
}

/***********************************************************
 * Name: inc_and_wrap_angle
 *
 * Arguments:
 *       GLfloat angle     current angle
 *       GLfloat angle_inc angle increment
 *
 * Description:   Increments or decrements angle by angle_inc degrees
 *                Wraps to 0 at 360 deg.
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc)
{
   angle += angle_inc;

   if (angle >= 360.0)
      angle -= 360.f;
   else if (angle <=0)
      angle += 360.f;

   return angle;
}

/***********************************************************
 * Name: inc_and_clip_distance
 *
 * Arguments:
 *       GLfloat distance     current distance
 *       GLfloat distance_inc distance increment
 *
 * Description:   Increments or decrements distance by distance_inc units
 *                Clips to range
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc)
{
   distance += distance_inc;

   if (distance >= 10.0f)
      distance = 10.f;
   else if (distance <= 1.0f)
      distance = 1.0f;

   return distance;
}

/***********************************************************
 * Name: redraw_scene
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Draws the model and calls eglSwapBuffers
 *                to render to screen
 *
 * Returns: void
 *
 ***********************************************************/
static void redraw_scene(CUBE_STATE_T *state)
{
   // Start with a clear screen
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   draw_wavefront(state->model, state->tex);

   eglSwapBuffers(state->display, state->surface);
}

/***********************************************************
 * Name: init_textures
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Initialise OGL|ES texture surfaces to use image
 *                buffers
 *
 * Returns: void
 *
 ***********************************************************/
static void init_textures(CUBE_STATE_T *state)
{
   // the texture containing the video
   glGenTextures(1, &state->tex);

   glBindTexture(GL_TEXTURE_2D, state->tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_SIZE_WIDTH, IMAGE_SIZE_HEIGHT, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   /* Create EGL Image */
   //eglImage = eglCreateImageKHR(
   eglImage = eglCreateImage(
                state->display,
                state->context,
                EGL_GL_TEXTURE_2D_KHR,
                (EGLClientBuffer)state->tex,
                0);
    
   if (eglImage == EGL_NO_IMAGE_KHR)
   {
      printf("eglCreateImageKHR failed.\n");
      exit(1);
   }

   // Start rendering
   pthread_create(&thread1, NULL, video_decode_test, eglImage);

   // setup overall texture environment
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glEnable(GL_TEXTURE_2D);

   // Bind texture surface to current vertices
   glBindTexture(GL_TEXTURE_2D, state->tex);
}
//------------------------------------------------------------------------------

static void exit_func(void)
// Function to be passed to atexit().
{
	
   thread_run = 0;
   pthread_join(thread1, NULL);
	
   if (eglImage != 0)
   {
      //if (!eglDestroyImageKHR(state->display, (EGLImageKHR) eglImage))
      if (!eglDestroyImage(state->display, eglImage))
         printf("eglDestroyImageKHR failed.");
   }

   // clear screen
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources
   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
   eglDestroySurface( state->display, state->surface );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );

   printf("\ncube closed\n");
} // exit_func()

void sig_handler(int signo) {

   terminate = 1;

}

//==============================================================================


// global variables declarations

static int device;
static drmModeRes *resources;
static drmModeConnector *connector;
static uint32_t connector_id;
static drmModeEncoder *encoder;
static drmModeModeInfo mode_info;
static drmModeCrtc *crtc;
static struct gbm_device *gbm_device;
static EGLDisplay display;
static EGLContext context;
static struct gbm_surface *gbm_surface;
static EGLSurface egl_surface;
       EGLConfig config;
       EGLint num_config;
       EGLint count=0;
       EGLConfig *configs;
       int config_index;
       int i;

static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb;

static EGLint attributes[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
		};

static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

struct gbm_bo *bo;
uint32_t handle;
uint32_t pitch;
int32_t fb;
uint64_t modifier;


static drmModeConnector *find_connector (drmModeRes *resources) {

for (i=0; i<resources->count_connectors; i++) {
  drmModeConnector *connector = drmModeGetConnector (device, resources->connectors[i]);
  if (connector->connection == DRM_MODE_CONNECTED) {return connector;}
  drmModeFreeConnector (connector);
  }
return NULL; // if no connector found
}

static drmModeEncoder *find_encoder (drmModeRes *resources, drmModeConnector *connector) {

if (connector->encoder_id) {return drmModeGetEncoder (device, connector->encoder_id);}
return NULL; // if no encoder found
}

static void swap_buffers () {

eglSwapBuffers (display, egl_surface);
bo = gbm_surface_lock_front_buffer (gbm_surface);
handle = gbm_bo_get_handle (bo).u32;
pitch = gbm_bo_get_stride (bo);
drmModeAddFB (device, mode_info.hdisplay, mode_info.vdisplay, 24, 32, pitch, handle, &fb);
drmModeSetCrtc (device, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);
if (previous_bo) {
  drmModeRmFB (device, previous_fb);
  gbm_surface_release_buffer (gbm_surface, previous_bo);
  }
previous_bo = bo;
previous_fb = fb;
}

static void draw (float progress) {

glClearColor (1.0f-progress, progress, 0.0, 1.0);
glClear (GL_COLOR_BUFFER_BIT);
swap_buffers ();
}

static int match_config_to_visual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count) {

EGLint id;
for (i = 0; i < count; ++i) {
  if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID,&id)) continue;
  if (id == visual_id) return i;
  }
return -1;
}

int init_gl () {

device = open ("/dev/dri/card1", O_RDWR);
resources = drmModeGetResources (device);
connector = find_connector (resources);
connector_id = connector->connector_id;
mode_info = connector->modes[0];
encoder = find_encoder (resources, connector);
crtc = drmModeGetCrtc (device, encoder->crtc_id);
drmModeFreeEncoder (encoder);
drmModeFreeConnector (connector);
drmModeFreeResources (resources);
gbm_device = gbm_create_device (device);
gbm_surface = gbm_surface_create (gbm_device, mode_info.hdisplay, mode_info.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
display = eglGetDisplay (gbm_device);
eglInitialize (display, NULL ,NULL);
eglBindAPI (EGL_OPENGL_API);
eglGetConfigs(display, NULL, 0, &count);
configs = malloc(count * sizeof *configs);
eglChooseConfig (display, attributes, configs, count, &num_config);
config_index = match_config_to_visual(display,GBM_FORMAT_XRGB8888,configs,num_config);
context = eglCreateContext (display, configs[config_index], EGL_NO_CONTEXT, context_attribs);
egl_surface = eglCreateWindowSurface (display, configs[config_index], gbm_surface, NULL);
free(configs);
eglMakeCurrent (display, egl_surface, egl_surface, context);

state->screen_width = mode_info.hdisplay;
state->screen_height = mode_info.vdisplay;
}
	
int end_gl () {
drmModeSetCrtc (device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector_id, 1, &crtc->mode);
drmModeFreeCrtc (crtc);
if (previous_bo) {
  drmModeRmFB (device, previous_fb);
  gbm_surface_release_buffer (gbm_surface, previous_bo);
  }
eglDestroySurface (display, egl_surface);
gbm_surface_destroy (gbm_surface);
eglDestroyContext (display, context);
eglTerminate (display);
gbm_device_destroy (gbm_device);

close (device);
return 0;
}

int main ()
{
   printf("Note: ensure you have sufficient gpu_mem configured\n");
   
   signal(SIGINT, sig_handler);

   // Clear application state
   memset( state, 0, sizeof( *state ) );
      
   // Start OGLES
   init_gl();
   init_ogl(state);

   // Setup the model world
   init_model_proj(state);

   // initialise the OGLES texture(s)
   init_textures(state);

   //state->model = cube_wavefront();
   state->model = load_wavefront("/opt/vc/src/hello_pi/hello_teapot/teapot.obj.dat", NULL);

   while (!terminate)
   {
      update_model(state);
      redraw_scene(state);
   }
   exit_func();
   return 0;
}

