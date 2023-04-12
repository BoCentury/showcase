
$("segtext_input").addEventListener("keydown", function(event) {
                                        // Number 13 is the "Enter" key on the keyboard
                                        if(event.keyCode === 13) {
                                            read_segment_text();
                                        }
                                    });

function $(id) {
    return document.getElementById(id);
}

function set_all_segments_to_same_size() {
    let seg_size = 360 / (the_segs.length);
    for(let i = 0; i < the_segs.length; i++) {
        let seg = the_segs[i];
        seg.start_angle = seg_size * i;
        seg.end_angle = seg_size * (i + 1);
        seg.size = seg.end_angle - seg.start_angle;
    }
    build_segment_elements();
    draw_wheel_at_segment_border();
}

function read_segment_text() {
    let segment_info = JSON.parse($("segtext_input").value);
    let segs = the_segs;
    segs.length = segment_info.length;
    $("segnum").max = segment_info.length;
    
    let prev_end_angle = 0;
    for(let i = 0; i < segment_info.length; i++) {
        let info = segment_info[i];
        if((typeof info.tts) !== "string") {
            alert(`Somethings wrong with the "tts" for segment ${i} (the first segment is segment 0)
                  Assuming no TTS.`);
            info.tts = "";
        }
        if((typeof info.angle) !== "number") {
            alert(`Somethings wrong with the "angle" for segment ${i} (the first segment is segment 0)
                  Setting segment size to 1.`);
            info.angle = prev_end_angle + 1;
        }
        
        let seg = new Segment();
        seg.tts = info.tts;
        seg.start_angle = prev_end_angle;
        seg.end_angle = info.angle;
        seg.size = seg.end_angle - seg.start_angle;
        prev_end_angle = seg.end_angle;
        
        seg.text = info.text;
        if(info.text_offset) seg.text_offset = info.text_offset;
        if(info.font_size) seg.text_font_size = info.font_size;
        
        segs[i] = seg;
    }
    
    
    build_segment_elements();
    draw_wheel_at_segment_border();
}

function old_read_segment_text() {
    let segment_info = JSON.parse($("segtext_input").value);
    let segs = the_segs;
    segs.length = segment_info.length;
    $("segnum").max = segment_info.length;
    
    let prev_end_angle = 0;
    for(let i = 0; i < segment_info.length; i++) {
        let info = segment_info[i];
        if((typeof info.text) !== "string") {
            alert(`Somethings wrong with the "text" for segment ${i} (the first segment is segment 0)
                  Assuming no TTS.`);
            info.text = "";
        }
        if((typeof info.size) !== "number") {
            if(i === segment_info.length - 1) {
                info.size = 360 - prev_end_angle;
            } else {
                alert(`Somethings wrong with the "size" for segment ${i} (the first segment is segment 0)
                      Setting segment size to 1.`);
                info.size = 1;
            }
        }
        
        let seg = new Segment();
        seg.tts = info.text;
        seg.start_angle = prev_end_angle;
        seg.end_angle = prev_end_angle + info.size;
        seg.size = seg.end_angle - seg.start_angle;
        prev_end_angle = seg.end_angle;
        
        seg.text = info.text.toUpperCase().replaceAll(" ", "\n");
        
        segs[i] = seg;
    }
    
    
    build_segment_elements();
    draw_wheel_at_segment_border();
}

// IMPORTANT IMPORTANT IMPORTANT IMPORTANT
// this function is only supposed to be used in the SE widget to get the tts text for each wheel to post in discord
function generate_tts_for_all_wheels() {
    let o = "```";
    for(let wheel_index = 1; wheel_index < wheels.length; wheel_index++) {
        let wheel = wheels[wheel_index];
        o += "Wheel " + wheel_index + ":\n";
        for(let i = 0; i < wheel.segments.length; i++) {
            let tts = wheel.segments[i].text;
            o += "\"" + tts + "\" ";
        }
        o += "\n\n";
    }
    o += "```";
    //navigator.clipboard.writeText(o);
    console.log(o);
}

function old_generate_segment_text() {
    let segments = new Array(the_segs.length);
    
    for(let i = 0; i < the_segs.length; i++) {
        let seg = the_segs[i];
        segments[i] = {
            "text": seg.tts,
            "size": seg.end_angle - seg.start_angle,
        };
    }
    
    let out = $("segtext_output");
    out.children[0].value = JSON.stringify(segments);
    out.style.visibility = "visible";
}

function generate_segment_text_with_visual_text() {
    let segments = new Array(the_segs.length);
    
    for(let i = 0; i < the_segs.length; i++) {
        let seg = the_segs[i];
        segments[i] = {
            "text": seg.text,
            "tts": seg.tts,
            "angle": seg.end_angle,
        };
        if(seg.text_offset) segments[i].text_offset = seg.text_offset;
        if(seg.text_font_size) segments[i].font_size = seg.text_font_size;
    }
    
    let out = $("segtext_output");
    out.children[0].value = JSON.stringify(segments);
    out.style.visibility = "visible";
}

function generate_segment_text() {
    let segments = new Array(the_segs.length);
    
    for(let i = 0; i < the_segs.length; i++) {
        segments[i] = {
            "tts": the_segs[i].tts,
            "angle": the_segs[i].end_angle,
        };
    }
    
    let out = $("segtext_output");
    out.children[0].value = JSON.stringify(segments);
    out.style.visibility = "visible";
}

function draw_wheel_at_segment_border() {
    let index = $("segnum").value;
    //console.log("draw_wheel_at_segment_border: " + index);
    
    if(index < 1) {
        the_wheel.rotation_angle = 0;
    } else {
        if(index > the_segs.length - 1) index = the_segs.length - 1;
        the_wheel.rotation_angle = -the_segs[index].end_angle;
    }
    the_wheel.rotation_angle -= image_rotation;
    
    the_wheel.redraw_whole_wheel();
    the_wheel.draw();
}

let image_rotation = 0;

function callback_image_rotation(value, from_range) {
    image_rotation = value;
    the_wheel.image_rotation = value;
    if(from_range) {
        $("imgrot_num").valueAsNumber = value;
    } else {
        $("imgrot_rng").valueAsNumber = value;
    }
    draw_wheel_at_segment_border();
}

function change_border_angle(left_seg, angle) {
    let right_seg = the_segs[left_seg.$i + 1];
    
    if(angle >= right_seg.end_angle - 1) {
        angle = right_seg.end_angle - 1;
        left_seg.size = (right_seg.end_angle - 1) - left_seg.start_angle;
        right_seg.size = 1;
    } else if(angle <= left_seg.start_angle + 1) {
        angle = left_seg.start_angle + 1;
        left_seg.size = 1;
        right_seg.size = right_seg.end_angle - (left_seg.start_angle + 1);
    } else {
        let change = angle - left_seg.end_angle;
        left_seg.size += change;
        right_seg.size -= change;
    }
    
    
    left_seg.end_angle = angle;
    right_seg.start_angle = angle;
    //the_wheel.update_segment_angles_from_sizes();
    draw_wheel_at_segment_border();
    
    left_seg.$range.valueAsNumber = left_seg.end_angle;
    left_seg.$end_angle.valueAsNumber = left_seg.end_angle;
    left_seg.$size.valueAsNumber = left_seg.size;
    
    if(left_seg.$i === the_segs.length - 1) {
        right_seg.$size.valueAsNumber = 360 - right_seg.start_angle;
    } else {
        right_seg.$size.valueAsNumber = right_seg.size;
    }
}

function on_angle_0_input() {
    change_border_angle(this.$seg, this.valueAsNumber);
}

function on_angle_range_input() {
    //console.log("angle_range: " + this.$seg.$i);
    change_border_angle(this.$seg, this.valueAsNumber);
}

function on_angle_number_input() {
    //console.log("angle_number: " + this.$seg.$i);
    change_border_angle(this.$seg, this.valueAsNumber);
}

function on_size_number_input() {
    //console.log("size_number: " + this.$seg.$i);
    if(this.value === "") this.valueAsNumber = 1;
    let seg = this.$seg;
    let change = this.valueAsNumber - seg.size;
    
    let trailing_seg_size = 360 - the_segs[the_segs.length - 1].start_angle;
    if(change >= trailing_seg_size - 1) {
        this.valueAsNumber = seg.size + trailing_seg_size - 1;
        change = this.valueAsNumber - seg.size;
    }
    
    seg.size = this.valueAsNumber;
    the_segs[the_segs.length - 1].size -= change;
    
    the_wheel.update_segment_angles_from_sizes();
    draw_wheel_at_segment_border();
    
    seg.$range.valueAsNumber = seg.end_angle;
    seg.$size.valueAsNumber = seg.end_angle;
    
    for(let i = seg.$i; i < the_segs.length; i++) {
        let it = the_segs[i];
        it.$range.valueAsNumber = it.end_angle;
        it.$end_angle.valueAsNumber = it.end_angle;
        it.$size.valueAsNumber = it.size;
    }
}

function tts_on_keydown(event) {
    if(event.keyCode === 13) { // 13 is enter
        speak(this.value);
    }
}

function callback_tts_button() {
    speak(this.$tts.value);
}

function callback_split_button() {
    let position = this.$seg.$i;
    for(let i = the_segs.length; i > position; i--) {
        the_segs[i] = the_segs[i - 1];
        the_segs[i].$i = i;
    }
    
    let right_split = the_segs[position + 1];
    let left_split = new Segment();
    the_segs[position] = left_split;
    
    left_split.$i = position;
    left_split.tts = right_split.tts;
    left_split.text = right_split.text;
    left_split.start_angle = right_split.start_angle;
    
    let middle = right_split.start_angle + ((right_split.end_angle - right_split.start_angle) / 2);
    left_split.end_angle = middle;
    right_split.start_angle = middle;
    
    left_split.size = left_split.end_angle - left_split.start_angle;
    right_split.size = right_split.end_angle - right_split.start_angle;
    
    build_segment_elements();
    draw_wheel_at_segment_border();
}

function move_segement(a, offset) {
    let b = a + offset;
    if(b >= 0 &&
       b < the_segs.length) {
        
        let tmp = the_segs[a];
        the_segs[a] = the_segs[b];
        the_segs[b] = tmp;
        
        the_wheel.update_segment_angles_from_sizes();
        
        // TODO: might have to swap angles
        build_segment_elements();
        draw_wheel_at_segment_border();
    }
}

function callback_move_up_button() {
    move_segement(this.$seg.$i, -1);
}

function callback_move_down_button() {
    move_segement(this.$seg.$i, 1);
}

function callback_close_button() {
    let position = this.$seg.$i;
    
    let right_split = the_segs[position + 1];
    let left_split = the_segs[position];
    
    right_split.start_angle = left_split.start_angle;
    right_split.size = right_split.end_angle - right_split.start_angle;
    
    for(let i = position + 1; i < the_segs.length; i++) the_segs[i - 1] = the_segs[i];
    the_segs.length -= 1;
    
    build_segment_elements();
    draw_wheel_at_segment_border();
}

let prev_selected_seg = null;

function callback_on_radio_change() {
    $("segnum").value = this.value;
    if(typeof this.$seg === "undefined") { // radio_0
        if(prev_selected_seg) prev_selected_seg.$row.className = "segment_info_row";
        prev_selected_seg = null;
    } else {
        this.$seg.$row.className = "segment_info_row segment_info_row_selected";
        if(prev_selected_seg) prev_selected_seg.$row.className = "segment_info_row";
        prev_selected_seg = this.$seg;
    }
    draw_wheel_at_segment_border();
}

function callback_on_line_input() {
    this.$seg.text = this.value.replaceAll("\\n", "\n");
    draw_wheel_at_segment_border();
}

function callback_on_tts_input() {
    this.$seg.tts = this.value;
}

function callback_on_text_offset_input() {
    let seg = this.$seg;
    if(this.value) {
        seg.text_offset = this.valueAsNumber;
    } else {
        seg.text_offset = null;
    }
    draw_wheel_at_segment_border();
}

function callback_on_font_size_input() {
    let seg = this.$seg;
    if(this.value) {
        seg.text_font_size = this.valueAsNumber;
    } else {
        seg.text_font_size = null;
    }
    draw_wheel_at_segment_border();
}

function set_wheel_number_property(name, e, fallback, skip_draw) {
    the_wheel[name] = (e.value) ? e.valueAsNumber : fallback;
    
    if(!skip_draw) draw_wheel_at_segment_border();
}

function set_seg_number_property(name, e, fallback) {
    e.$seg[name] = (e.value) ? e.valueAsNumber : fallback;
    
    if(!skip_draw) draw_wheel_at_segment_border();
}

function set_wheel_text_property(name, e, fallback, skip_draw) {
    the_wheel[name] = (e.value) ? e.value : fallback;
    
    if(!skip_draw) draw_wheel_at_segment_border();
}

function set_seg_text_property(name, e, fallback) {
    e.$seg[name] = (e.value) ? e.value : fallback;
    
    if(!skip_draw) draw_wheel_at_segment_border();
}

function build_segment_elements() {
    $("radio_0").oninput = callback_on_radio_change;
    
    let body = $("segment_info");
    body.innerHTML = "";
    if(the_segs.length < 1) return;
    
    for(let i = 0; i < the_segs.length; i++) {
        let seg = the_segs[i];
        seg.$i = i;
        
        let radio = document.createElement('input');
        radio.type = "radio";
        radio.title = "Turn the wheel so that the arrow points at the 'right-hand' border of this segment.\nA good way to line things up properly.";
        radio.name = "showseg";
        radio.oninput = callback_on_radio_change;
        radio.value = i;
        radio.$seg = seg;
        seg.$radio = radio;
        
        let range = document.createElement('input');
        range.type = "range";
        range.title = "The angle of the 'right-hand' border of this segment";
        range.min = 0;
        range.max = 360;
        range.value = seg.end_angle;
        range.oninput = on_angle_range_input;
        range.$seg = seg;
        seg.$range = range;
        
        let end_angle = document.createElement('input');
        end_angle.type = "number";
        end_angle.title = "The angle of the 'right-hand' border of this segment";
        end_angle.min = 0;
        end_angle.max = 360;
        end_angle.step = 0.1;
        end_angle.value = seg.end_angle;
        end_angle.oninput = on_angle_number_input;
        end_angle.$seg = seg;
        seg.$end_angle = end_angle;
        
        let size = document.createElement('input');
        size.type = "number";
        size.title = "The size of the segment in degrees [0-360]";
        size.min = 0;
        size.max = 360;
        size.step = 0.1;
        size.value = seg.size;
        size.oninput = on_size_number_input;
        size.$seg = seg;
        seg.$size = size;
        
        let text = document.createElement('input');
        text.type = "text";
        text.title = "Type the text to be shown in this segment here.\nType '\\n' to make a new line.";
        text.placeholder = "Type the visual text here";
        if(typeof seg.text == "string") text.value = seg.text.replaceAll("\n", "\\n");
        text.oninput = callback_on_line_input;
        text.$seg = seg;
        seg.$text = text;
        
        let tts = document.createElement('input');
        tts.type = "text";
        tts.title = "Type the TTS text for this segment here.\nLeave blank for no TTS.";
        tts.placeholder = "Type the TTS text here";
        tts.value = seg.tts;
        tts.oninput = callback_on_tts_input;
        tts.addEventListener("keydown", tts_on_keydown);
        tts.$seg = seg;
        seg.$tts = tts;
        
        let tts_button = document.createElement('button');
        tts_button.textContent = "Test TTS";
        tts_button.$tts = tts;
        tts_button.onclick = callback_tts_button;
        
        let text_offset = document.createElement('input');
        text_offset.type = "number";
        text_offset.title = "Text offset for this segment from center and out.\nCurrent default is " + the_wheel.text_offset;
        text_offset.valueAsNumber = (seg.text_offset !== null) ? seg.text_offset : the_wheel.text_offset;
        text_offset.step = 1;
        text_offset.oninput = callback_on_text_offset_input;
        text_offset.$seg = seg;
        
        let font_size = document.createElement('input');
        font_size.type = "number";
        font_size.title = "Text font size for this segment.\nCurrent default is " + the_wheel.text_font_size;
        font_size.valueAsNumber = (seg.text_font_size !== null) ? seg.text_font_size : the_wheel.text_font_size;
        font_size.step = 1;
        font_size.oninput = callback_on_font_size_input;
        font_size.$seg = seg;
        
        
        let split_button = document.createElement('button');
        split_button.title = "Splits the segment in two to make another segment";
        split_button.textContent = "Split";
        split_button.className = "smol_button";
        split_button.$seg = seg;
        split_button.onclick = callback_split_button;
        
        
        let close_button = document.createElement('button');
        close_button.title = "Deletes this segment.\n";
        close_button.textContent = "X";
        close_button.className = "smol_button";
        close_button.$seg = seg;
        close_button.onclick = callback_close_button;
        
        let move_up_button = document.createElement('button');
        move_up_button.title = "Move this segment up on the list.";
        move_up_button.textContent = "\u25B2"; // up arrow
        move_up_button.className = "smol_button";
        move_up_button.$seg = seg;
        move_up_button.onclick = callback_move_up_button;
        
        let move_down_button = document.createElement('button');
        move_down_button.title = "Move this segment down on the list.";
        move_down_button.textContent = "\u25BC"; // down arrow
        move_down_button.className = "smol_button";
        move_down_button.$seg = seg;
        move_down_button.onclick = callback_move_down_button;
        
        if(i === 0) move_up_button.disabled = true;
        if(i === the_segs.length - 1) move_down_button.disabled = true;
        
        let row0 = document.createElement("div");
        let row1 = document.createElement("div");
        row0.appendChild(radio);
        row0.appendChild(range);
        row0.appendChild(end_angle);
        row0.appendChild(size);
        row0.appendChild(split_button);
        row0.appendChild(close_button);
        row0.appendChild(move_up_button);
        row0.appendChild(move_down_button);
        row0.className = "segmen_info_row_0";
        
        row1.appendChild(tts);
        row1.appendChild(tts_button);
        row1.appendChild(text);
        row1.appendChild(text_offset);
        row1.appendChild(font_size);
        row1.className = "segment_info_row_1";
        
        let row = document.createElement("div");
        row.className = "segment_info_row";
        row.appendChild(row0);
        row.appendChild(row1);
        seg.$row = row;
        body.appendChild(row);
        //body.appendChild(document.createElement("br"));
    }
    
    the_segs[the_segs.length - 1].$range.disabled = true;
    the_segs[the_segs.length - 1].$end_angle.disabled = true;
    the_segs[the_segs.length - 1].$size.disabled = true;
    the_segs[the_segs.length - 1].$size.valueAsNumber = 360 - the_segs[the_segs.length - 1].end_angle;
}

function reset_wheel() {
    the_wheel.stop_animation(true);
    the_wheel.rotation_angle = 0;
    the_wheel.redraw_whole_wheel();
    the_wheel.draw();
}

function spin_wheel() {
    the_wheel.animation.stop_angle = Math.floor(Math.random() * 360);
    the_wheel.start_animation();
}

function speak(text) {
    if(typeof text === "string" && text !== "") {
        const tts = new Audio();
        tts.src = "https://api.streamelements.com/kappa/v2/speech?voice=Brian&text=" + encodeURIComponent(text);
        tts.volume = 0.9;
        tts.play();
    }
}

function spin_done_callback() {
    let winning_segment = the_wheel.get_indicated_segment();
    speak(winning_segment.tts);
}

function make_segments(text) {
    let tmpsegments = JSON.parse(`[${text}]`);
    let segments = [{}];
    for(let i = 0; i < tmpsegments.length; i++) {
        segments[i] = new Segment(tmpsegments[i]);
    }
    
    // Fill in segement sizes
    let total_degrees_reserved = 0;
    let number_of_segments_with_reserved_size = 0;
    
    for(let i = 0; i < segments.length; i++) {
        if(segments[i].size !== null) {
            total_degrees_reserved += segments[i].size;
            number_of_segments_with_reserved_size++;
        }
    }
    
    let unreserved_degrees = 360 - total_degrees_reserved;
    let degrees_each = 0;
    if(unreserved_degrees > 0) degrees_each = unreserved_degrees / (segments.length - number_of_segments_with_reserved_size);
    
    let at = 0;
    for(let i = 0; i < segments.length; i++) {
        segments[i].start_angle = at;
        at += segments[i].size ? segments[i].size : degrees_each;
        segments[i].end_angle = at;
    }
    
    return segments;
}

const the_wheel = new Winwheel({
                                   draw_mode: 'image',
                                   //rotation_angle: 0,
                                   outer_radius: 398, // 147 is radius of tip of arrow
                                   //inner_radius:   80, // 147 is radius of tip of arrow
                                   draw_text: true,
                                   text_colour: "#ffffff",
                                   animation: {
                                       type: 'spinToStop',
                                       duration: 3,
                                       spins: 8,
                                       callback_finished: spin_done_callback,
                                   },
                                   pointer_guide: {
                                       display: false,
                                       stroke_style: 'green',
                                       line_width: 3
                                   }
                               });

let wheel_0_segments = make_segments(`
                                     {"text":"L","size":135.0},
                                     {"text":"VIP. wow","size":12.7},
                                     {"text":"L"}
                                     `);
let wheel_1_segments = make_segments(`
                                     {"text":"drop sights","size":33.1},
                                     {"text":"empty mag","size":55.8},
                                     {"text":"L","size":46.2},
                                     {"text":"VIP. wow","size":12.9},
                                     {"text":"self nade","size":65.3},
                                     {"text":"stand still","size":58.4},
                                     {"text":"altt tab","size":56.5},
                                     {"text":"drop sights"}
                                     `);
let wheel_2_segments = make_segments(`
                                     {"text":"drop helm","size":33.1},
                                     {"text":"single fire","size":55.8},
                                     {"text":"use ult","size":59.1},
                                     {"text":"self nade","size":65.3},
                                     {"text":"weapon trade","size":58.3},
                                     {"text":"drop big meds","size":56.5},
                                     {"text":"drop helm"}
                                     `);
let wheel_3_segments = make_segments(`
                                     {"text":"hip fire only","size":33.1},
                                     {"text":"gun game","size":55.8},
                                     {"text":"no shield heal","size":31.5},
                                     {"text":"unbind reload","size":27.6},
                                     {"text":"drop all ammo of one type","size":65.3},
                                     {"text":"same armor","size":58.3},
                                     {"text":"drop attachments","size":56.5},
                                     {"text":"hip fire only"}
                                     `);
let wheel_4_segments = make_segments(`
                                     {"text":"drop bag","size":33.1},
                                     {"text":"swap character","size":55.8},
                                     {"text":"deathbox loot only","size":59.1},
                                     {"text":"drop inventory","size":38.2},
                                     {"text":"alt f4","size":27.1},
                                     {"text":"unbind shift","size":58.3},
                                     {"text":"right mouse shoot","size":56.5},
                                     {"text":"drop bag"}
                                     `);
let wheel_5_segments = make_segments(`
                                     {"text":"light ammo","size":33.1},
                                     {"text":"shotgun ammo","size":55.8},
                                     {"text":"energy ammo","size":59.1},
                                     {"text":"arrows","size":65.3},
                                     {"text":"sniper ammo","size":58.3},
                                     {"text":"heavy ammo","size":56.5},
                                     {"text":"light ammo"}
                                     `);
let wheel_6_segments = make_segments(`
                                     {"text":"ignore one roll","size":33.1},
                                     {"text":"rebind shift","size":55.8},
                                     {"text":"pick up dropped items","size":59.1},
                                     {"text":"phone a friend","size":65.3},
                                     {"text":"L","size":51.0},
                                     {"text":"VIP. wow","size":7.4},
                                     {"text":"left mouse shoot","size":56.5},
                                     {"text":"ignore one roll"}
                                     `);

//the_wheel.segments    = wheel_2_segments;

the_wheel.segments = [new Segment()];
the_wheel.update_segment_angles_from_sizes();
$("segnum").max = the_wheel.segments.length;

const the_segs = the_wheel.segments;

the_wheel.draw_segment_outlines_over_image = true;
the_wheel.outer_radius = 293;

the_wheel.text_orientation = "vertical";
the_wheel.text_offset = 110;
the_wheel.text_font_family = "Gadugi Regular";
the_wheel.text_font_weight = "normal";
the_wheel.text_font_size = 53;
the_wheel.text_colour = "#d6c24e";
//the_wheel.text_line_width = 5;

build_segment_elements();

function set_mode(hitbox) {
    if(hitbox) {
        the_wheel.draw_text = false;
        the_wheel.segment_outline_colour = "#ff0000";
        the_wheel.segment_outline_width = 1;
    } else {
        the_wheel.draw_text = true;
        the_wheel.segment_outline_colour = "#d6c24e";
        the_wheel.segment_outline_width = 5;
    }
    draw_wheel_at_segment_border();
}
set_mode(false);

/*
[{"tts":"drop helm","angle":33.1},{"tts":"single fire","angle":88.9},{"tts":"use ult","angle":148},{"tts":"self nade","angle":213.3},{"tts":"weapon trade","angle":271.6},{"tts":"drop big meds","angle":328.1},{"tts":"drop helm","angle":360}]
*/

//
$("segtext_input").value = `[{"text":"DROP\\nHELM","tts":"drop helm","angle":60.0},
                             {"text":"SINGLE\\nFIRE","tts":"single fire","angle":120.0},
                             {"text":"USE\\nULT","tts":"use ult","angle":180.0},
                             {"text":"SELF\\nNADE","tts":"self nade","angle":240.0},
                             {"text":"WEAPON\\nTRADE","tts":"weapon trade","angle":300.0},
                             {"text":"DROP\\nBIG\\nMEDS","tts":"drop big meds","angle":360,"text_offset":130}]`;
read_segment_text();


function grayscale(image, bPlaceImage) {
    let myCanvas = document.createElement("canvas");
    let myCanvasContext = myCanvas.getContext("2d");
    
    let imgWidth = image.width;
    let imgHeight = image.height;
    myCanvas.width = imgWidth;
    myCanvas.height = imgHeight;
    myCanvasContext.drawImage(image, 0, 0);
    
    // This function cannot be called if the image is not rom the same domain.
    // You'll get security error if you do.
    let imageData = myCanvasContext.getImageData(0, 0, imgWidth, imgHeight);
    
    // This loop gets every pixels on the image and
    for(let y = 0; y < imageData.height; y++) {
        for(let x = 0; x < imageData.width; x++) {
            let index = (y * 4) * imageData.width + (x * 4);
            let r = imageData.data[index];
            let g = imageData.data[index + 1];
            let b = imageData.data[index + 2];
            let average = (r + g + b) / 3;
            imageData.data[index] = average * 3;
            imageData.data[index + 1] = 0;
            imageData.data[index + 2] = 0;
        }
    }
    
    myCanvasContext.putImageData(imageData, 0, 0);
    
    if(bPlaceImage) {
        let myDiv = document.createElement("div");
        myDiv.appendChild(myCanvas);
        $("container").appendChild(myCanvas);
    }
    return myCanvas.toDataURL();
}

$("fileInputWheel").onchange = function(e) {
    let URL = window.webkitURL || window.URL;
    let url = URL.createObjectURL(e.target.files[0]);
    let img = new Image();
    img.onload = function() {
        the_wheel.wheel_image = img;
        the_wheel.redraw_whole_wheel();
        the_wheel.draw();
    };
    img.src = url;
};

let arrow_offset_x = 0;
let arrow_offset_y = -51;

function update_arrow_position() {
    let arrow = $("arrow");
    // + 8 cause of 8 pixel margin being annoying
    arrow.style.left = (the_wheel.center_x + 20 - arrow.width / 2 + 8 + arrow_offset_x) + "px";
    arrow.style.top = (the_wheel.center_x + 20 - arrow.height + 8 - arrow_offset_y) + "px";
}

$("fileInputArrow").onchange = function(e) {
    let URL = window.webkitURL || window.URL;
    let url = URL.createObjectURL(e.target.files[0]);
    let arrow = $("arrow");
    arrow.onload = update_arrow_position;
    arrow.src = url;
};

function scale_seg_img(img, scale) {
    img.width = img.og_width * scale;
    img.height = img.og_height * scale;
    draw_wheel_at_segment_border();
}

function callback_on_seg_image_load() {
    let scale = 0.312;
    this.og_width = this.width;
    this.og_height = this.height;
    this.width = this.og_width * scale;
    this.height = this.og_height * scale;
    draw_wheel_at_segment_border();
}

$("fileInputSegImages").onchange = function(e) {
    let URL = window.webkitURL || window.URL;
    let min_length = (e.target.files.length < (the_segs.length)) ? e.target.files.length : the_segs.length;
    for(let i = 0; i < min_length; i++) {
        let seg = the_segs[i];
        seg.img_data = new Image();
        seg.img_data.onload = callback_on_seg_image_load;
        seg.img_data.src = URL.createObjectURL(e.target.files[i]);
        seg.image_offset = 50;
    }
};

$("save").onclick = function() {
    let link = document.createElement("a");
    link.download = "wheel.png";
    link.href = $("canvas").toDataURL();
    link.click();
};