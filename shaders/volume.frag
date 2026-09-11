#version 430 core

in vec3 vLocalPos;

// Textures 
uniform sampler3D uVolumeTex;
uniform sampler3D uGradientTex;
uniform sampler1D uTransferFunc;

// Camera / Transform
uniform vec3 uCameraPos;       // world space
uniform mat4 uModel;
uniform mat4 uModelInv;

// Render parameters
uniform int   uRenderMode;     // 0 = DVR, 1 = Isosurface
uniform int   uNumSteps;       // 512 for DVR, 1024 for ISO
uniform float uIsoThreshold;   // isosurface density value

// Visibility window
uniform float uMinVisibility;  // default 0.0
uniform float uMaxVisibility;  // default 1.0

// Lighting 
uniform bool  uLightingEnabled;
uniform vec3  uLightDir;       // world-space, normalized
uniform vec3  uAmbientColor;   // scene ambient tint (matches background)

// Slice plane
uniform bool  uSliceEnabled;
uniform mat4  uSliceMatrix;    // world-to-plane-local

// Jitter
uniform float uJitterOffset;

out vec4 FragColor;

// Constants
const float SQRT3 = 1.73205080757;
const float OPACITY_THRESHOLD = 1.0 - 1.0 / 255.0;
const float REF_STEP_SIZE = SQRT3 / 512.0;  // reference for opacity correction

// Ray-AABB intersection (slab method)
vec2 intersectAABB(vec3 origin, vec3 dir)
{
    vec3 invDir = 1.0 / dir;
    vec3 t0 = (vec3(-0.5) - origin) * invDir;
    vec3 t1 = (vec3( 0.5) - origin) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    return vec2(max(tNear, 0.0), tFar);
}

// Hash noise for jitter
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Phong lighting
vec3 phongLight(vec3 color, vec3 normal, vec3 lightDir, vec3 viewDir,
                float ambientStrength, float specIntensity, float specPower)
{
    // Flip normal if back-facing
    normal *= sign(dot(normal, viewDir));

    // Ambient: tinted by scene color to match background lighting
    vec3 ambient = uAmbientColor * ambientStrength;

    // Diffuse: slightly cool white directional light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = vec3(0.95, 0.97, 1.0) * diff;

    // Specular highlight
    vec3 refl  = reflect(-lightDir, normal);
    float spec = pow(max(dot(refl, viewDir), 0.0), specPower) * specIntensity;

    // Rim light: environment bounce on edges facing away from camera
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 3.0) * 0.25;
    vec3 rimColor = uAmbientColor * rim;

    return color * (ambient + diffuse) + vec3(spec) + rimColor;
}

void main()
{
    // Ray in local (model) space
    vec3 camLocal = (uModelInv * vec4(uCameraPos, 1.0)).xyz;
    vec3 rayDir   = normalize(vLocalPos - camLocal);

    // Intersect with unit AABB
    vec2 tHit = intersectAABB(camLocal, rayDir);
    float tNear = tHit.x;
    float tFar  = tHit.y;

    if (tNear >= tFar) discard;

    float stepSize = SQRT3 / float(uNumSteps);

    // Jitter start for anti-banding
    float jitter = hash(gl_FragCoord.xy + vec2(uJitterOffset)) * stepSize;
    tNear += jitter;

    // Light direction in model space
    vec3 lightLocal = normalize((uModelInv * vec4(uLightDir, 0.0)).xyz);

    // DVR (front-to-back compositing)
    if (uRenderMode == 0)
    {
        vec4 accum = vec4(0.0);

        for (int i = 0; i < uNumSteps; i++)
        {
            float t = tNear + float(i) * stepSize;
            if (t > tFar) break;

            vec3 pos = camLocal + rayDir * t;
            vec3 tc  = pos + 0.5;  // [-0.5,0.5] → [0,1]

            // Bounds check
            if (any(lessThan(tc, vec3(0.0))) || any(greaterThan(tc, vec3(1.0))))
                continue;

            // Slice plane clipping
            if (uSliceEnabled)
            {
                vec4 worldPos  = uModel * vec4(pos, 1.0);
                vec4 planePos  = uSliceMatrix * worldPos;
                if (planePos.z > 0.0) continue;
            }

            // Sample density
            float density = texture(uVolumeTex, tc).r;

            // Visibility window
            if (density < uMinVisibility || density > uMaxVisibility)
                continue;

            // Transfer function lookup
            vec4 src = texture(uTransferFunc, density);
            if (src.a < 0.001) continue;

            // Opacity correction for step size independence
            src.a = 1.0 - pow(1.0 - src.a, stepSize / REF_STEP_SIZE);

            // Optional lighting
            if (uLightingEnabled && src.a > 0.01)
            {
                vec3 grad = texture(uGradientTex, tc).xyz;
                float gradMag = length(grad);
                if (gradMag > 0.01)
                {
                    vec3 normal = grad / gradMag;
                    src.rgb = phongLight(src.rgb, normal, -lightLocal,
                                         -rayDir, 0.3, 0.3, 32.0);
                }
            }

            // Front-to-back over operator
            src.rgb *= src.a;
            accum += (1.0 - accum.a) * src;

            // Early ray termination
            if (accum.a > OPACITY_THRESHOLD) break;
        }

        FragColor = accum;
    }
    // Isosurface (first-hit)
    else
    {
        float prevDensity = 0.0;

        for (int i = 0; i < uNumSteps; i++)
        {
            float t = tNear + float(i) * stepSize;
            if (t > tFar) break;

            vec3 pos = camLocal + rayDir * t;
            vec3 tc  = pos + 0.5;

            if (any(lessThan(tc, vec3(0.0))) || any(greaterThan(tc, vec3(1.0)))) {
                prevDensity = 0.0;
                continue;
            }

            // Slice plane clipping
            if (uSliceEnabled)
            {
                vec4 worldPos  = uModel * vec4(pos, 1.0);
                vec4 planePos  = uSliceMatrix * worldPos;
                if (planePos.z > 0.0) {
                    prevDensity = 0.0;
                    continue;
                }
            }

            float density = texture(uVolumeTex, tc).r;

            // Visibility window
            if (density < uMinVisibility || density > uMaxVisibility) {
                prevDensity = density;
                continue;
            }

            // Detect threshold crossing
            if (density >= uIsoThreshold && prevDensity < uIsoThreshold)
            {
                // Refine hit position via linear interpolation
                float ratio = (uIsoThreshold - prevDensity)
                            / (density - prevDensity + 0.0001);
                float tHitPt = (tNear + float(i - 1) * stepSize) + ratio * stepSize;
                vec3 hitPos  = camLocal + rayDir * tHitPt;
                vec3 hitTC   = hitPos + 0.5;

                // Gradient for surface normal
                vec3 grad = texture(uGradientTex, hitTC).xyz;
                float gradMag = length(grad);

                if (gradMag < 0.005) {
                    prevDensity = density;
                    continue;
                }

                vec3 normal = grad / gradMag;

                // TF colour at threshold
                vec4 baseColor = texture(uTransferFunc, uIsoThreshold);

                // Phong lighting
                vec3 viewDir = -rayDir;
                vec3 lit = phongLight(baseColor.rgb, normal, -lightLocal,
                                      viewDir, 0.2, 0.15, 32.0);

                FragColor = vec4(lit, 1.0);
                return;
            }

            prevDensity = density;
        }

        discard;
    }
}
