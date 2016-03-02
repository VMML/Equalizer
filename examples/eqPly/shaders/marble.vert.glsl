
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

void main()
{
    v_texCoord3D = gl_Vertex.xyz;

    vec4 ecPos = gl_ModelViewProjectionMatrix * gl_Vertex;
    vec4 positionEye = normalize( gl_ModelViewMatrix * gl_Vertex );
     
    // Set varyings for subscatter FS
    if( gl_LightSource[0].position.w == 0.0 )
        // directional light
        lightPos = normalize( gl_LightSource[0].position ).xyz;
    else
        // point light
        lightPos = normalize( gl_LightSource[0].position - positionEye ).xyz;
    worldNormal = gl_NormalMatrix * gl_Normal;     
    eyeVec = -ecPos.xyz;
    lightVec = lightPos - ecPos.xyz;
    vertPos = ecPos.xyz;
     
    //Transform vertex by modelview and projection matrices
    gl_Position = ecPos;
    
    // pass the vertex colors on to the fragment shader
    gl_FrontColor = gl_Color;
}
