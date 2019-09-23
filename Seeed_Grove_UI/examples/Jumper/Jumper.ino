
#define private public
#ifndef __DEBUG__
#ifdef KENDRYTE_K210
  #include<SPIClass.h>
#else
  #include<SPI.h>
#endif

#include"string.h"
#include"Seeed_FS.h"
#include"SD/Seeed_SD.h"
#include"TFT_eSPI.h"
#define println(...)        Serial.println(__VA_ARGS__)
#define serial_begin(baud)  Serial.begin(baud)
#else
#include"string.h"
#include"stdlib.h"
#include"stdio.h"
typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
struct TFT_eSPI{
    template<class ... arg>
    void pushImage(arg ... list){

    }
    void fillScreen(uint32_t){}
};
#define serial_begin(baud)
#define println(...)   
#define delay(...)
uint32_t random(uint32_t min, uint32_t max){
    return uint32_t(rand() % max + min);
}
#endif

#define PIN_JUMP          0
#define PIN_BREAKING_OUT  1        
#define SERIAL Serial
#define SDCARD_SPI SPI1
#define SDCARD_MISO_PIN PIN_SPI1_MISO
#define SDCARD_MOSI_PIN PIN_SPI1_MOSI
#define SDCARD_SCK_PIN PIN_SPI1_SCK
#define SDCARD_SS_PIN PIN_SPI1_SS
constexpr int16_t  screen_width = 320;
constexpr int16_t  screen_height = 240;
TFT_eSPI  tft;

struct static_memory_t{
    static_memory_t(){
        ptr = mem;
    }
    template<class type>
    type * alloc(int32_t bytes){
        int8_t * tmp = ptr;
        ptr += bytes;
        return (type *)tmp;
    }
    int8_t * ptr;
    int8_t   mem[1024 * 32];
}static_memory;

struct raw_image{
    int16_t  width;
    int16_t  height;
    uint8_t * ptr(){
        return (uint8_t *)(this + 1);
    }
    uint8_t get(int16_t x, int16_t y){
        return this->ptr()[y * width + x];
    }
};

enum location : uint8_t{
    left_top,
    left_bottom,
    right_top,
    right_bottom,
};

struct point{
    int16_t      x;
    int16_t      y;
    point(){}
    point(int16_t x, int16_t y): 
        x(x), y(y){
    }
};

enum pix_type: uint8_t{
    pix_type_ignore,
    pix_type_actor          = 1 << 0,
    pix_type_breaking_out   = 1 << 1,
    pix_type_block          = 1 << 2,
    pix_type_bird           = 1 << 3,
};

struct object : point{
    raw_image ** raw;
    pix_type     type;
    location     loc;
    uint8_t      behavior;
    int8_t       x_move_speed;
    int8_t       y_move_speed;
    bool         in_range;
    object(){}
    object(
        pix_type     type, 
        point        p, 
        raw_image ** raw, 
        int8_t       x_move_speed = 0,
        int8_t       y_move_speed = 0,
        uint8_t      behavior = 0, 
        location     loc = location::left_bottom):
        type(type), raw(raw),
        x_move_speed(x_move_speed), y_move_speed(y_move_speed),
        behavior(behavior), loc(loc){
        in_range = true;
        set_point(p);
    }
    void move(){
        x += x_move_speed;
        y += y_move_speed;
    }
    void set_point(point value){
        *(point *)this = value;
    }
};

struct painter{
    uint8_t toggle(object & obj, uint8_t a, uint8_t b){
        obj.behavior = obj.behavior == a ? b : a;
        return draw(obj);
    }
    uint8_t draw(object & obj){
        uint8_t     mask = 0;
        uint8_t     transparent = 0xff;
        raw_image * img = obj.raw[obj.behavior];
        int16_t     x_start, x_end, y_start, y_end;
        int16_t     x_p = 0, y_p = 0;
        switch (obj.loc){
        case left_top    : x_start = obj.x; y_start = obj.y; break;
        case left_bottom : x_start = obj.x; y_start = obj.y - img->height; break;
        case right_top   : x_start = obj.x - img->width; y_start = obj.y; break;
        case right_bottom: 
        default:           x_start = obj.x - img->width; y_start = obj.y - img->height; break;
        }

        x_end = x_start + img->width;
        y_end = y_start + img->height;

        if (
            not_in_range(x_start, y_start) &&
            not_in_range(x_start, y_end  ) &&
            not_in_range(x_end  , y_start) &&
            not_in_range(x_end  , y_end  )){
            obj.in_range = false;
            println("not in range");
            return mask;
        }
        if (x_start < 0){
            x_p = -x_start;
            x_start = 0;
        }
        if (x_end > screen_width){
            x_end = screen_width;
        }
        if (y_start < 0){
            y_p = -y_start;
            y_start = 0;
        }
        if (y_end > screen_height){
            y_end = screen_height;
        }
        for (uint16_t y = y_start, x_tmp = x_p; y < y_end; y++, y_p++){
            for (uint16_t x = x_start, x_p = x_tmp; x < x_end; x++, x_p++){
                auto value = img->get(x_p, y_p);
                if (value == transparent){
                    continue;
                }
                
                buffer[y][x] = value;

                if (obj.type == pix_type_ignore){
                    continue;
                }
                if (collide[y][x] == pix_type_ignore){
                    collide[y][x] = obj.type;
                }
                else if (collide[y][x] != obj.type){
                    mask |= collide[y][x];
                }
            }
        }
        return mask;
    }
    void draw_line_h(int16_t x, int16_t y, int16_t length){

    }
    void clean(){
        memset(collide, pix_type_ignore, sizeof(collide));
        memset(buffer, -1, sizeof(buffer));
    }
    void flush(){
        tft.pushImage(0, 0, screen_width, screen_height, (uint8_t *)buffer);
    }
private:
    bool not_in_range(int16_t x, int16_t y){
        return !(0 <= x && x < screen_width && 0 <= y && y < screen_height);
    }
    uint8_t  buffer[screen_height][screen_width];
    pix_type collide[screen_height][screen_width];
};

volatile bool pushed_jump = false;
volatile bool pushed_breaking_out = false;

#ifndef __DEBUG__
void push_jump(){
    if (digitalRead(PIN_JUMP) == HIGH){
        pushed_jump = true;
    }
}
void push_breaking_out(){
    if (digitalRead(PIN_BREAKING_OUT) == HIGH){
        pushed_breaking_out = true;
    }
}
#endif

struct jumper{
private:
    enum class game{
        over,
        go_on,
    };
    enum speed{
        x_bird_speed  = -12,
        x_block_speed = -12,
        x_cloud_speed = -7,
        x_breaking_out_speed = 20,
    };
    enum hung_raw{
        jump0,
        jump1,
        jump2,
        jump3,
        jump4,
        fall0,
        fall1,
        fall2,
        fall3,
        max_jump_path,
    };
    enum{
        max_wire_count  = 1,
        max_bird_count  = 4,
        max_block_count = 3,
        max_cloud_count = 3,
        max_object_count = 10,
    };
    enum actor_behavior{
        run0, run1, run_mode = run1, 
        breaking_out, jump, failure, max_actor_behavior,
    };
    enum bird_behavior{
        fly0, fly1,
        max_bird_behavior,
    };
    enum block_behavior{
        small_block, big_block,
        max_block_type,
    };
    raw_image * raw_actor[max_actor_behavior];
    raw_image * raw_bird[max_bird_behavior];
    raw_image * raw_block[max_block_type];
    raw_image * raw_cloud[1];
    raw_image * raw_game_over[1];
    raw_image * raw_wire[1];
    painter     pan;
    object      actor;
    object      game_over;
    object      wire[1];
    object      bird[max_bird_count];
    object      block[max_block_count];
    object      cloud[max_cloud_count];
    point       jump_path[max_jump_path];
    point       start_of_block;
    point       start_of_breaking_out;
    int8_t      actor_last_behavior;
    int8_t      current_bird_count;
    int8_t      current_block_count;
    int8_t      current_cloud_count;
    int8_t      current_wire_count;
    int8_t      current_breaking_out_count;
    int8_t      current_jump_state;
public:
    void begin(){
        println("load image");
        raw_actor[run0]         = load_image("jumper/run0.G8");
        raw_actor[run1]         = load_image("jumper/run1.G8");
        raw_actor[jump]         = load_image("jumper/jump.G8");
        raw_actor[breaking_out] = load_image("jumper/breaking_out.G8");
        raw_actor[failure]      = load_image("jumper/failure.G8");
        raw_block[small_block]  = load_image("jumper/small_block.G8");
        raw_block[big_block]    = load_image("jumper/big_block.G8");
        raw_bird [fly0]         = load_image("jumper/fly0.G8");
        raw_bird [fly1]         = load_image("jumper/fly1.G8");
        raw_cloud[0]            = load_image("jumper/cloud.G8");
        raw_wire [0]            = load_image("jumper/wire.G8");
        println("finish load");
    }
    void play(){
        game flag = game::go_on;
        reset();

        while(flag != game::over){
            clean();
            calc();
            frame(flag);
            flush();
            delay(30);
        }
        do{
            show_game_over();
            delay(30);
        }while(!pushed_jump && !pushed_breaking_out);
    }
private:
    void frame(game & flag){
        uint8_t collide_mask = 0;
        flag = game::go_on;
        for (
            int16_t base = 160, step = 20, end = base + step * 3; 
            base < end; 
            base += step){
            pan.draw_line_h(0, base, screen_width);
        }
        for (size_t i = 0; i < current_cloud_count; i++) {
            pan.draw(cloud[i]);
        }
        if (actor.behavior <= run_mode){
            static_assert(run0 == 0 && run1 == 1, "please don't touch those value");
            pan.toggle(actor, run0, run1);
        }
        else{
            pan.draw(actor);
            if (actor.behavior == breaking_out){
                actor.behavior = actor_last_behavior;
            }
        }
        for (size_t i = 0; i < current_block_count; i++) {
            collide_mask |= pan.draw(block[i]);
        }
        for (size_t i = 0; i < current_bird_count; i++) {
            collide_mask |= pan.toggle(bird[i], fly0, fly1);
        }
        if (collide_mask & pix_type_actor){
            flag = game::over;
        }
        for (size_t i = 0; i < current_wire_count; i++) {
            if (pan.draw(wire[i]) & pix_type_bird){
                println("breaking it");
                bird->in_range = false;
                wire[i].in_range = false;
            }
        }
    }
    void calc(){
        auto rate = 10;
        auto can_generate = [this](object * obj, uint32_t current_count){
            if (current_count > 0){
                auto & o = obj[current_count - 1];
                auto b = o.raw[o.behavior];
                return o.x < screen_width - actor.raw[actor.behavior]->width * 4;
            }
            else{
                return true;
            }
        };
        if (pushed_jump){
            pushed_jump = false;
            actor.behavior = jump;
        }
        if (pushed_breaking_out){
            pushed_breaking_out = false;
            if (current_wire_count == 0){
                current_wire_count = 1;
                actor_last_behavior = actor.behavior;
                actor.behavior = breaking_out;
                wire[0] = object(
                    pix_type_breaking_out,
                    point(actor.x + raw_actor[0]->width, actor.y - 16),
                    raw_wire,
                    x_breaking_out_speed);
                println("generate wire");
            }
        }
        if (actor.behavior == jump){
            actor.set_point(jump_path[current_jump_state++]);
            if (current_jump_state == max_jump_path){
                current_jump_state = 0;
                actor.behavior = run0;
            }
        }

        //generate a cloud
        if (current_cloud_count < max_cloud_count && can_generate(cloud, current_cloud_count)){
            println("generate a cloud");
            cloud[current_cloud_count++] = object(
                pix_type_ignore,
                point(screen_width - 1, random(50, 130)),
                raw_cloud,
                x_cloud_speed);
        }

        //generate a block
        bool has_generate_block = current_block_count == 0;
        if (current_block_count < max_block_count){
            has_generate_block = can_generate(block, current_block_count);
            if (has_generate_block && random(0, rate) == 0) {
                println("generate a block");
                block[current_block_count++] = object(
                    pix_type_block,
                    start_of_block,
                    raw_block,
                    x_block_speed, 0,
                    random(0, 4) ? small_block : big_block);
            }
        }

        //generate a bird
        else if (
            has_generate_block == false && 
            current_bird_count < max_bird_count && 
            random(0, rate) == 0 && 
            can_generate(bird, current_bird_count)){
            println("generate a bird");
            auto i = random(0, max_jump_path);
            bird[current_bird_count++] = object(
                pix_type_bird,
                point(screen_width - 1, jump_path[i].y - 16),
                raw_bird, 
                x_bird_speed);
        }
        
        current_bird_count  = move(bird , current_bird_count );
        current_wire_count  = move(wire , current_wire_count);
        current_cloud_count = move(cloud, current_cloud_count);
        current_block_count = move(block, current_block_count);
    }
    void reset(){
        println("reset variable");
        size_t start_height = 180;
        for (
            size_t i = 0, a = 12, step = a * max_jump_path / 2, h = start_height;
            i < max_jump_path;
            i++, step -= a, h -= step){
            jump_path[i] = point(40, h);
        }
        actor = object(pix_type_actor, jump_path[0], raw_actor);
        current_bird_count = 0;
        current_block_count = 0;
        current_cloud_count = 0;
        current_jump_state = 0;
        current_wire_count = 0;
        start_of_block = point(screen_width - 1, start_height);
        println("finish reset");
    }
    void clean(){ pan.clean(); }
    void flush(){ pan.flush(); }
    void show_game_over(){
        game flag;
        clean();
        actor.behavior = failure;
        frame(flag);
        flush();
    }
    void turn_light_on(){

    }
    void turn_light_off(){
        
    }
    raw_image * load_image(const char * path){
        #ifndef __DEBUG__
        File f = SD.open(path, FILE_READ);
        if (!f){
            println("read file error");
            return nullptr;
        }
        int32_t     size = f.size();
        raw_image * mem = static_memory.alloc<raw_image>(size);
        f.read(mem, size);
        return mem;
        #else
        FILE * f = fopen(path, "r");
        if (!f){
            println("read file error");
            return nullptr;
        }
        raw_image ri;
        fread(&ri, sizeof(ri), 1, f);
        int32_t     size = sizeof(ri) + ri.height * ri.width;
        raw_image * mem = static_memory.alloc<raw_image>(size);
        mem[0] = ri;
        fread(mem->ptr(), size, 1, f);
        return mem;
        #endif
    }
    int8_t move(object * obj, int8_t count){
        uint8_t in_range[max_object_count];
        uint8_t end = 0;

        //keep the object index which in range.
        for (size_t i = 0; i < count; i++){
            obj[i].move();
            if (obj[i].in_range){
                in_range[end++] = i;
            }
        }

        //compress
        for (size_t i = 0; i < end; i++){
            auto index = in_range[i];
            if (i == index){
                continue;
            }
            obj[i] = obj[index];
        }
        return end;
    }
};

jumper game;

void setup(){
    #ifndef __DEBUG__
    serial_begin(115200);
    while (!SERIAL) {
      ;// wait for SERIAL port to connect. Needed for native USB port only
    }
    println("Initializing SD card...");
    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        println("initialization failed!");
        while (1);
    }

    println("initialization done.");
    tft.init();
    tft.setRotation(1);
    pinMode(PIN_JUMP, INPUT);
    pinMode(PIN_BREAKING_OUT, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_JUMP), push_jump, CHANGE);  
    attachInterrupt(digitalPinToInterrupt(PIN_BREAKING_OUT), push_breaking_out, CHANGE);
    #endif
    game.begin();
}
void loop() {
    game.play();
}

#ifdef __DEBUG__
int main(){
    jumper::game flag;
    game.begin();
    while(true){
        loop();
    }
    return 0;
}
#endif
