#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>

static constexpr int   WIN_W    = 1280;
static constexpr int   WIN_H    = 720;
static constexpr float UI_H     = 80.f;
static constexpr float SPEED_X  = 200.f;
static constexpr float SPEED_W  = 220.f;
static constexpr float RAYS_X   = 550.f;
static constexpr float RAYS_W   = 220.f;
static constexpr float SLIDER_Y = WIN_H - 50.f;

static const char* LENS_SHADER = R"(
uniform sampler2D starfield;
uniform vec2      bhPos;
uniform vec2      resolution;
uniform float     bhRS;

void main() {
    vec2 uv  = gl_FragCoord.xy / resolution;
    vec2 bhN = bhPos / resolution;
    vec2  delta = uv - bhN;
    float dist  = length(delta);
    float normD = dist * min(resolution.x, resolution.y);
    float eRing = bhRS * 2.8 / min(resolution.x, resolution.y);
    float strength = (bhRS * bhRS * 3.5) / (normD * normD + 0.001);
    strength = min(strength, 0.45);
    vec2 lensedUV = uv + normalize(bhN - uv) * strength * dist;
    lensedUV = clamp(lensedUV, 0.0, 1.0);
    vec4 starColor = texture2D(starfield, lensedUV);
    float ringW = 0.012;
    float ringD = abs(dist - eRing);
    float ring  = exp(-ringD * ringD / (ringW * ringW)) * 1.8;
    starColor.rgb += vec3(1.0, 0.92, 0.7) * ring;
    float photonR = bhRS * 1.5 / min(resolution.x, resolution.y);
    float photon  = exp(-abs(dist - photonR) * 180.0) * 0.6;
    starColor.rgb += vec3(1.0, 0.75, 0.3) * photon;
    float rsN = bhRS / min(resolution.x, resolution.y);
    if (dist < rsN) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    float darkness = smoothstep(rsN, rsN * 3.0, dist);
    starColor.rgb *= darkness;
    gl_FragColor = vec4(starColor.rgb, 1.0);
}
)";

struct BlackHole { float x, y, mass, rs; };

struct Ray {
    float x, y, vx, vy, baseSpeed;
    float initVx, initVy;
    float closestApproach, deflectionAngle;
    bool  alive, captured;
    std::vector<sf::Vector2f> trail;
    sf::Color color;
};

struct JetParticle {
    float x, y, vx, vy, life, maxLife;
    sf::Color color;
};

static float randf(float lo, float hi) {
    return lo + (hi-lo)*((float)rand()/(float)RAND_MAX);
}

static Ray makeRay(int index, int total, float speed, float bhX, float bhY) {
    Ray r;
    r.alive=true; r.captured=false;
    r.closestApproach=99999.f; r.deflectionAngle=0.f;
    r.trail.clear();
    r.baseSpeed=speed;
    float spread=300.f;
    float step=(total>1)?spread*2.f/(total-1):0.f;
    float yOff=(total>1)?-spread+index*step:0.f;
    r.x=0.f; r.y=bhY+yOff;
    r.vx=r.initVx=speed;
    r.vy=r.initVy=0.f;
    int type=index%3;
    if      (type==0) r.color=sf::Color(255,220,120,220);
    else if (type==1) r.color=sf::Color(160,210,255,200);
    else              r.color=sf::Color(255,255,255,190);
    return r;
}

static sf::Texture makeStarfield(int w, int h) {
    std::vector<uint8_t> px(w*h*4,0);
    auto hash=[](float x,float y)->float{
        float v=std::sin(x*127.1f+y*311.7f)*43758.5453f;
        return v-std::floor(v);
    };
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        float h1=hash((float)x,(float)y);
        float h2=hash((float)x*1.3f,(float)y*0.7f);
        int idx=(y*w+x)*4;
        px[idx+0]=3;px[idx+1]=3;px[idx+2]=12;px[idx+3]=255;
        if (h1>0.987f) {
            float bright=(h1-0.987f)/0.013f;
            uint8_t v=(uint8_t)(bright*bright*255.f);
            if      (h2>0.7f){px[idx+0]=v;px[idx+1]=v;px[idx+2]=(uint8_t)(v*0.7f);}
            else if (h2>0.4f){px[idx+0]=(uint8_t)(v*0.8f);px[idx+1]=(uint8_t)(v*0.9f);px[idx+2]=v;}
            else             {px[idx+0]=v;px[idx+1]=v;px[idx+2]=v;}
        }
        float nb=hash((float)x*0.02f,(float)y*0.02f);
        if (nb>0.65f){
            float t=(nb-0.65f)/0.35f*0.06f;
            px[idx+0]=std::min(255,px[idx+0]+(int)(t*40));
            px[idx+1]=std::min(255,px[idx+1]+(int)(t*15));
            px[idx+2]=std::min(255,px[idx+2]+(int)(t*80));
        }
    }
    sf::Texture tex({(unsigned)w,(unsigned)h});
    tex.update(px.data());
    tex.setSmooth(true);
    return tex;
}

static void drawSlider(sf::RenderWindow& w, float x, float y,
                       float width, float t, sf::Color knobCol) {
    sf::RectangleShape track({width,4.f});
    track.setPosition({x,y-2.f});
    track.setFillColor(sf::Color(80,80,80,200));
    w.draw(track);
    sf::RectangleShape fill({width*t,4.f});
    fill.setPosition({x,y-2.f});
    fill.setFillColor(sf::Color(200,140,40,220));
    w.draw(fill);
    sf::CircleShape knob(9.f);
    knob.setOrigin({9.f,9.f});
    knob.setPosition({x+width*t,y});
    knob.setFillColor(knobCol);
    knob.setOutlineThickness(2.f);
    knob.setOutlineColor(sf::Color::White);
    w.draw(knob);
}

struct Preset {
    std::string name;
    int numRays; float speedT;
    float bhX, bhY, mass, rs;
};

int main() {
    std::srand((unsigned)std::time(nullptr));

    sf::RenderWindow window(
        sf::VideoMode({WIN_W,WIN_H}),
        "Black Hole Simulation",
        sf::Style::Titlebar|sf::Style::Close
    );
    window.setFramerateLimit(60);

    sf::Font font;
    bool hasFont=font.openFromFile("C:/Windows/Fonts/arial.ttf");

    sf::Texture starTex=makeStarfield(WIN_W,WIN_H);

    sf::Shader lensShader;
    bool hasShader=sf::Shader::isAvailable()&&
                   lensShader.loadFromMemory(LENS_SHADER,sf::Shader::Type::Fragment);
    if (hasShader) {
        lensShader.setUniform("starfield",  starTex);
        lensShader.setUniform("resolution", sf::Glsl::Vec2(WIN_W,WIN_H));
    }

    BlackHole bh={WIN_W/2.f,WIN_H/2.f-30.f,22000.f,32.f};

    float speedT=0.3f, raysT=0.2f;
    int   numRays=6;
    float raySpeed=1.5f;

    auto calcSpeed=[](float t){return 0.5f+t*9.5f;};
    auto calcRays =[](float t){return 1+(int)(t*19.f);};

    std::vector<Ray> rays;
    std::vector<JetParticle> jets;

    int   capturedCount=0, escapedCount=0;
    float minApproach=99999.f, maxDeflection=0.f;

    std::vector<Preset> presets={
        {"Default",    6, 0.3f, WIN_W/2.f,WIN_H/2.f-30.f,22000.f,32.f},
        {"E. Ring",   16, 0.25f,WIN_W/2.f,WIN_H/2.f-30.f,28000.f,38.f},
        {"Photon Orb", 1, 0.18f,WIN_W/2.f,WIN_H/2.f-30.f,26000.f,34.f},
        {"Light Speed",10,1.0f, WIN_W/2.f,WIN_H/2.f-30.f,22000.f,32.f},
    };
    int activePreset=0;

    sf::RenderTexture trailRT;
    trailRT.resize({WIN_W,WIN_H});
    trailRT.clear(sf::Color::Transparent);

    sf::RectangleShape fadeRect({(float)WIN_W,(float)WIN_H});
    fadeRect.setFillColor(sf::Color(0,0,0,10));

    sf::RectangleShape fullScreen({(float)WIN_W,(float)WIN_H});
    fullScreen.setTexture(&starTex);

    bool draggingSpeed=false, draggingRays=false;

    float presetStartX=860.f, presetY=WIN_H-UI_H+10.f;
    float presetW=88.f, presetH=28.f, presetGap=8.f;

    float jetTimer=0.f;

    auto respawnAll=[&](){
        trailRT.clear(sf::Color::Transparent);
        rays.resize(numRays);
        for (int i=0;i<numRays;i++)
            rays[i]=makeRay(i,numRays,raySpeed,bh.x,bh.y);
        jets.clear();
    };
    respawnAll();

    sf::Clock clock, simClock;
    int fpsCount=0; float fpsTimer=0.f;

    while (window.isOpen()) {
        float dt=clock.restart().asSeconds();
        dt=std::min(dt,0.033f);
        fpsTimer+=dt; fpsCount++;

        while (auto ev=window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();
            if (const auto* k=ev->getIf<sf::Event::KeyPressed>())
                if (k->code==sf::Keyboard::Key::Escape) window.close();

            if (const auto* mb=ev->getIf<sf::Event::MouseButtonPressed>()) {
                float mx=(float)mb->position.x, my=(float)mb->position.y;

                // Right click — move black hole
                if (mb->button==sf::Mouse::Button::Right && my<WIN_H-UI_H) {
                    bh.x=mx; bh.y=my;
                    capturedCount=0; escapedCount=0;
                    minApproach=99999.f; maxDeflection=0.f;
                    respawnAll();
                }

                // Sliders
                if (mx>=SPEED_X-10&&mx<=SPEED_X+SPEED_W+10&&
                    my>=SLIDER_Y-15&&my<=SLIDER_Y+15) draggingSpeed=true;
                if (mx>=RAYS_X-10&&mx<=RAYS_X+RAYS_W+10&&
                    my>=SLIDER_Y-15&&my<=SLIDER_Y+15) draggingRays=true;

                // Preset buttons
                for (int i=0;i<(int)presets.size();i++) {
                    float bx=presetStartX+i*(presetW+presetGap);
                    if (mx>=bx&&mx<=bx+presetW&&my>=presetY&&my<=presetY+presetH) {
                        activePreset=i;
                        auto& p=presets[i];
                        bh={p.bhX,p.bhY,p.mass,p.rs};
                        speedT=p.speedT; numRays=p.numRays;
                        raysT=(float)(p.numRays-1)/19.f;
                        raySpeed=calcSpeed(speedT);
                        capturedCount=0; escapedCount=0;
                        minApproach=99999.f; maxDeflection=0.f;
                        respawnAll();
                    }
                }
            }
            if (ev->getIf<sf::Event::MouseButtonReleased>()) {
                draggingSpeed=false; draggingRays=false;
            }
            if (const auto* mm=ev->getIf<sf::Event::MouseMoved>()) {
                float mx=(float)mm->position.x;
                if (draggingSpeed){
                    speedT=std::max(0.f,std::min(1.f,(mx-SPEED_X)/SPEED_W));
                    raySpeed=calcSpeed(speedT);
                    respawnAll();
                }
                if (draggingRays){
                    raysT=std::max(0.f,std::min(1.f,(mx-RAYS_X)/RAYS_W));
                    numRays=calcRays(raysT);
                    respawnAll();
                }
            }
        }

        if (fpsTimer>=1.f){
            window.setTitle("Black Hole Simulation  |  FPS: "+std::to_string(fpsCount));
            fpsTimer=0;fpsCount=0;
        }

        // Jet particles
        jetTimer+=dt;
        if (jetTimer>0.04f){
            jetTimer=0.f;
            for (int pole=-1;pole<=1;pole+=2){
                JetParticle jp;
                jp.x=bh.x+randf(-4.f,4.f); jp.y=bh.y;
                jp.vx=randf(-0.3f,0.3f);
                jp.vy=pole*randf(2.5f,5.5f);
                jp.maxLife=jp.life=randf(0.8f,1.8f);
                jp.color=(pole<0)?sf::Color(80,180,255,220):sf::Color(255,100,200,200);
                jets.push_back(jp);
            }
        }
        for (auto& jp:jets){jp.life-=dt;jp.x+=jp.vx;jp.y+=jp.vy;jp.vx+=randf(-0.05f,0.05f);}
        jets.erase(std::remove_if(jets.begin(),jets.end(),
            [](const JetParticle& j){return j.life<=0.f;}),jets.end());

        // Update rays
        for (int i=0;i<(int)rays.size();i++){
            auto& r=rays[i];
            if (!r.alive){
                if (r.captured) capturedCount++;
                else            escapedCount++;
                if (r.closestApproach<minApproach)   minApproach=r.closestApproach;
                if (r.deflectionAngle>maxDeflection) maxDeflection=r.deflectionAngle;
                r=makeRay(i,numRays,raySpeed,bh.x,bh.y);
                continue;
            }
            r.trail.push_back({r.x,r.y});
            if (r.trail.size()>600) r.trail.erase(r.trail.begin());

            float dx=bh.x-r.x, dy=bh.y-r.y;
            float dist=std::sqrt(dx*dx+dy*dy);
            if (dist<r.closestApproach) r.closestApproach=dist;
            if (dist<bh.rs){r.alive=false;r.captured=true;continue;}

            float grav=std::min(bh.mass/(dist*dist),8.f);
            r.vx+=(dx/dist)*grav*dt;
            r.vy+=(dy/dist)*grav*dt;

            float spd=std::sqrt(r.vx*r.vx+r.vy*r.vy);
            if (spd>0.001f){r.vx=(r.vx/spd)*r.baseSpeed;r.vy=(r.vy/spd)*r.baseSpeed;}

            float dot=r.vx*r.initVx+r.vy*r.initVy;
            float mag=r.baseSpeed*r.baseSpeed;
            if (mag>0.f){
                float cosA=std::max(-1.f,std::min(1.f,dot/mag));
                r.deflectionAngle=std::acos(cosA)*180.f/3.14159f;
            }

            float dNorm=std::min(1.f,r.deflectionAngle/90.f);
            r.color.r=(uint8_t)(160+dNorm*95);
            r.color.g=(uint8_t)(200-dNorm*150);
            r.color.b=(uint8_t)(255-dNorm*200);

            r.x+=r.vx; r.y+=r.vy;
            if ((r.x<-50||r.x>WIN_W+50||r.y<-50||r.y>WIN_H-UI_H+50)&&dist>WIN_W)
                r.alive=false;
        }

        // Trails
        trailRT.draw(fadeRect,sf::BlendAlpha);
        for (const auto& r:rays){
            if (r.trail.size()<2) continue;
            int n=(int)r.trail.size();
            for (int i=1;i<n;i++){
                float ft=(float)i/n;
                sf::Vertex line[2];
                line[0].position=r.trail[i-1];
                line[1].position=r.trail[i];
                line[0].color=sf::Color(r.color.r,r.color.g,r.color.b,
                    (uint8_t)(ft*(float)(i-1)/n*r.color.a*0.9f));
                line[1].color=sf::Color(r.color.r,r.color.g,r.color.b,
                    (uint8_t)(ft*r.color.a));
                trailRT.draw(line,2,sf::PrimitiveType::Lines);
            }
        }
        trailRT.display();

        // ── Render ───────────────────────────────────────────────────────
        window.clear(sf::Color(3,3,12));

        if (hasShader){
            lensShader.setUniform("bhPos",sf::Glsl::Vec2(bh.x,WIN_H-bh.y));
            lensShader.setUniform("bhRS", bh.rs);
            sf::RenderStates rs; rs.shader=&lensShader;
            window.draw(fullScreen,rs);
        } else {
            sf::Sprite fb(starTex); window.draw(fb);
        }

        sf::Sprite trailSprite(trailRT.getTexture());
        window.draw(trailSprite,sf::BlendAdd);

        // Jets
        for (const auto& jp:jets){
            float t2=jp.life/jp.maxLife;
            float r2=3.f*t2+1.f;
            sf::CircleShape dot(r2);
            dot.setOrigin({r2,r2});
            dot.setPosition({jp.x,jp.y});
            dot.setFillColor(sf::Color(jp.color.r,jp.color.g,jp.color.b,(uint8_t)(t2*t2*200)));
            window.draw(dot);
        }

        // BH glow
        struct G{float r;sf::Color c;};
        G glows[]={
            {bh.rs*5.f,  sf::Color(50,25,5,18)},
            {bh.rs*3.5f, sf::Color(120,60,12,38)},
            {bh.rs*2.3f, sf::Color(190,105,22,65)},
            {bh.rs*1.6f, sf::Color(235,165,45,105)},
            {bh.rs*1.15f,sf::Color(255,205,110,155)},
        };
        for (auto& g:glows){
            sf::CircleShape ring(g.r);
            ring.setOrigin({g.r,g.r});
            ring.setPosition({bh.x,bh.y});
            ring.setFillColor(g.c);
            window.draw(ring);
        }
        for (int i=0;i<3;i++){
            float rr=bh.rs*(1.05f+i*0.18f);
            sf::CircleShape ring(rr);
            ring.setOrigin({rr,rr});
            ring.setPosition({bh.x,bh.y});
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(i==0?2.f:1.f);
            uint8_t b=(uint8_t)(200-i*60);
            ring.setOutlineColor(sf::Color(255,b,50,b));
            window.draw(ring);
        }
        sf::CircleShape core(bh.rs);
        core.setOrigin({bh.rs,bh.rs});
        core.setPosition({bh.x,bh.y});
        core.setFillColor(sf::Color::Black);
        window.draw(core);

        for (const auto& r:rays){
            if (!r.alive||r.trail.empty()) continue;
            sf::CircleShape dot(2.5f);
            dot.setOrigin({2.5f,2.5f});
            dot.setPosition({r.x,r.y});
            dot.setFillColor(sf::Color::White);
            window.draw(dot);
        }

        // ── UI Panel ─────────────────────────────────────────────────────
        sf::RectangleShape uiPanel({(float)WIN_W,UI_H});
        uiPanel.setPosition({0.f,WIN_H-UI_H});
        uiPanel.setFillColor(sf::Color(8,8,18,230));
        window.draw(uiPanel);
        sf::RectangleShape div({(float)WIN_W,1.f});
        div.setPosition({0.f,WIN_H-UI_H});
        div.setFillColor(sf::Color(80,60,20,200));
        window.draw(div);

        drawSlider(window,SPEED_X,SLIDER_Y,SPEED_W,speedT,sf::Color(255,200,80));
        drawSlider(window,RAYS_X, SLIDER_Y,RAYS_W, raysT, sf::Color(100,200,255));

        if (hasFont){
            auto txt=[&](const std::string& s,float x,float y,
                         sf::Color c,unsigned sz=12u){
                sf::Text tx(font,s,sz);
                tx.setFillColor(c);
                tx.setPosition({x,y});
                window.draw(tx);
            };

            // Stats
            txt("STATS",14.f,WIN_H-UI_H+6.f,sf::Color(180,180,200),11);
            txt("Captured: "+std::to_string(capturedCount),
                14.f,WIN_H-UI_H+20.f,sf::Color(255,120,80),11);
            txt("Escaped:  "+std::to_string(escapedCount),
                14.f,WIN_H-UI_H+33.f,sf::Color(100,220,150),11);
            if (minApproach<99990.f){
                std::ostringstream o;
                o<<std::fixed; o.precision(1);
                o<<"Min approach: "<<minApproach<<"px";
                txt(o.str(),14.f,WIN_H-UI_H+46.f,sf::Color(200,200,120),11);
            }
            {
                std::ostringstream o;
                o<<std::fixed; o.precision(1);
                o<<"Max deflect: "<<maxDeflection<<" deg";
                txt(o.str(),14.f,WIN_H-UI_H+59.f,sf::Color(180,150,255),11);
            }

            // Speed
            txt("SPEED",SPEED_X,WIN_H-UI_H+8.f,sf::Color(180,140,50));
            std::string sVal=speedT>0.95f?"Speed of Light":
                             speedT>0.6f ?"Relativistic":
                             speedT>0.3f ?"Fast":"Slow";
            txt(sVal,SPEED_X+SPEED_W/2.f-20.f,WIN_H-UI_H+8.f,sf::Color(255,220,120));

            // Rays
            txt("LIGHT RAYS",RAYS_X,WIN_H-UI_H+8.f,sf::Color(60,160,200));
            txt(std::to_string(numRays)+" rays",
                RAYS_X+RAYS_W/2.f-10.f,WIN_H-UI_H+8.f,sf::Color(140,220,255));

            // Preset buttons
            txt("PRESETS",presetStartX,WIN_H-UI_H+6.f,sf::Color(160,160,180),11);
            for (int i=0;i<(int)presets.size();i++){
                float bx=presetStartX+i*(presetW+presetGap);
                bool active=(i==activePreset);
                sf::RectangleShape btn({presetW,presetH});
                btn.setPosition({bx,presetY});
                btn.setFillColor(active?sf::Color(60,45,15,220):sf::Color(25,25,40,200));
                btn.setOutlineThickness(1.f);
                btn.setOutlineColor(active?sf::Color(220,170,50):sf::Color(60,60,80));
                window.draw(btn);
                txt(presets[i].name,bx+5.f,presetY+6.f,
                    active?sf::Color(255,220,100):sf::Color(160,160,180),11);
            }

            txt("Drag sliders | Right-click to move BH | ESC quit",
                SPEED_X,WIN_H-18.f,sf::Color(70,70,90,160),11);

            sf::Text credit(font,"Developed by Sankalp S. Kulkarni",12);
            credit.setFillColor(sf::Color(140,140,160,200));
            float cx=WIN_W-credit.getLocalBounds().size.x-14.f;
            credit.setPosition({cx,WIN_H-22.f});
            window.draw(credit);
        }

        window.display();
    }
    return 0;
}