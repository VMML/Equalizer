
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 
    
varying vec3 v_texCoord3D;
varying vec3 worldNormal, eyeVec, lightVec, vertPos, lightPos;

float time = 1.0;

float MaterialThickness = 0.25;
vec3 ExtinctionCoefficient = vec3(0.25, 0.25, 0.25);
vec4 LightColor = vec4(0.5, 0.5, 0.5, 1.0);
//vec4 BaseColor = gl_Color;
vec4 SpecColor = vec4(LightColor.x * 0.5, LightColor.y * 0.25, LightColor.x * 0.15, LightColor.w);
float SpecPower = 2.0;
float RimScalar = 0.25;

vec4 Ia = vec4(0.11, 0.10, 0.09, 1.0);
vec4 Kd = vec4(1.0, 0.25, 0.125, 1.0);

const float C1 = 0.429043;
const float C2 = 0.511664;
const float C3 = 0.743125;
const float C4 = 0.886227;
const float C5 = 0.247708;

// Constants for Old Town Square lighting
const vec3 L00  = vec3( 0.871297,  0.875222,  0.864470);
const vec3 L1m1 = vec3( 0.175058,  0.245335,  0.312891);
const vec3 L10  = vec3( 0.034675,  0.036107,  0.037362);
const vec3 L11  = vec3(-0.004629, -0.029448, -0.048028);
const vec3 L2m2 = vec3(-0.120535, -0.121160, -0.117507);
const vec3 L2m1 = vec3( 0.003242,  0.003624,  0.007511);
const vec3 L20  = vec3(-0.028667, -0.024926, -0.020998);
const vec3 L21  = vec3(-0.077539, -0.086325, -0.091591);
const vec3 L22  = vec3(-0.161784, -0.191783, -0.219152);

float halfLambert(in vec3 vect1, in vec3 vect2)
{
    float product = dot(vect1,vect2);
    return product * 0.5 + 0.5;
}
 
float blinnPhongSpecular(in vec3 normalVec, in vec3 lightVec, in float specPower)
{
    vec3 halfAngle = normalize(normalVec + lightVec);
    return pow(clamp(0.0,1.0,dot(normalVec,halfAngle)),specPower);
}

vec3 marble_color (float x)
{
   vec3 col;
   x = 0.5*(x+1.);          // transform -1<x<1 to 0<x<1
   x = sqrt(x);             // make x fall of rapidly...
   x = sqrt(x);
   x = sqrt(x);
   col = vec3(.45 + .45 * x);
   col.b*=0.97;
   return col;
}

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

float turbulence (vec3 P, int numFreq)
{
   float val = 0.0;
   float freq = 1.0;
   for (int i=0; i<numFreq; i++) {
      val += abs (noise (P*freq) / freq);
      freq *= 2.07;
   }
   return val;
}

void main()
{
    vec3 tnorm = normalize(worldNormal);
    vec3 diffuse = C1 * L22 * (tnorm.x * tnorm.x - tnorm.y * tnorm.y) +
                   C3 * L20 * tnorm.z * tnorm.z +
                   C4 * L00 -
                   C5 * L20 +
                   2.0 * C1 * L2m2 * tnorm.x * tnorm.y +
                   2.0 * C1 * L21  * tnorm.x * tnorm.z +
                   2.0 * C1 * L2m1 * tnorm.y * tnorm.z +
                   2.0 * C2 * L11  * tnorm.x +
                   2.0 * C2 * L1m1 * tnorm.y +   
                   2.0 * C2 * L10  * tnorm.z;

    float amplitude = 16.0;
    const int roughness = 24;     // noisiness of veins (#octaves in turbulence)
 
    float t = 6.28 * v_texCoord3D.x * 4.0;
    t += amplitude * turbulence(v_texCoord3D.xyz, roughness);
    // replicate over rows of tiles (wont be identical, because noise is depending on all coordinates of the input vector):
    t = sin(t);
    vec4 marbleColor = gl_Color * vec4(marble_color(t), 1.0);

//  vec3 l = normalize(vec3(1.0));
//  float lambert = vec3(clamp(dot(worldNormal, l), 0.0, 1.0));
    vec3 n = normalize(worldNormal);
    gl_FragColor = clamp(marbleColor * vec4(diffuse, 1.0) + Ia * 0.5, vec4(0.0, 0.0, 0.0, 1.0), vec4(1.0, 1.0, 1.0, 1.0));
}
