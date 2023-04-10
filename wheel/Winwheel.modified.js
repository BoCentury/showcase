//~ START

//
// This is a modified version of Douglas McKechie's WinWheel library.
// Original available here: https://github.com/zarocknz/javascript-winwheel
//
// See end of file for license information (MIT)
//


//  TODO: test the default_options+loop vs this.canvas_id = "canvas";...+just looping over everything in options or just Object.assign(this, options);


// By default the wheel is drawn if canvas object exists on the page, but can pass false as second parameter if don't want this to happen.
function Winwheel(options, draw_wheel) {
    let default_options = {
        canvas_id: "canvas", // ID of the canvas to draw the wheel on
        center_x: null, // Default null means center of canvas
        center_y: null, // Default null means center of canvas
        outer_radius: null, // Radius of the outside of the wheel. Deafult null means min(canvas_width, canvas_height)/2
        inner_radius: 0, // Normally 0. Allows the creation of rings / doughnuts if set to value > 0. Should not exceed outer radius
        draw_mode: "code", // Possible values: "code", "image", "segment_image". Default "code" means segments are drawn using canvas arc() function
        rotation_angle: 0,
        text_font_family: "Arial", // Segment text font. Should be web safe fonts
        text_font_size: 20, // Size of the segment text
        text_font_weight: "bold", // Font weight
        text_orientation: "horizontal", // Either horizontal, vertical
        text_alignment: "center", // Possible values: "center", "inner", "outer"
        text_offset: null, // Margin between the inner or outer of the wheel (depends on text_alignment)
        text_colour: "black",
        text_outline_colour: null, // Default null means no outline
        text_outline_width: 1,
        segment_colour: "silver", // Segment background colour
        segment_outline_colour: "black", // null = not drawn
        segment_outline_width: 1,
        clear_the_canvas: true, // When set to true the canvas will be cleared before the wheel is drawn.
        draw_segment_outlines_over_image: false, // For draw_mode "image" and "segment_image"
        draw_text: true, // Default is true in draw_mode "code" and false in "image" and "segment_image"
        pointer_angle: 0, // Direction of the pointer that indicates which segment the wheel lands on. Default is up
        wheel_image: null, // Image object to be drawn in draw_mode "image"
        image_scale: 1, // Scale factor for wheel_image
        image_rotation: 0, // Rotation angle for wheel_image
        image_offset_x: 0, // X offset for wheel_image in case it isn't properly centered and doesn't spin smoothly
        image_offset_y: 0, // Y offset for wheel_image in case it isn't properly centered and doesn't spin smoothly
        image_direction: "N", // Used when draw_mode is segment_image. Default is north, can also be (E)ast, (S)outh, (W)est.
        scale_factor: 1, // Set by the responsive function. Used in many calculations to scale the wheel.
    };
    
    if(options != null) {
        Object.assign(default_options, options);
    }
    for(let key in default_options) {
        this[key] = default_options[key];
    }
    
    this.canvas = null;
    this.ctx = null;
    this.offscreen_ctx = null;
    
    if(this.canvas_id) {
        this.canvas = document.getElementById(this.canvas_id);
        
        if(this.canvas) {
            if(this.center_x == null) this.center_x = this.canvas.width  / 2;
            if(this.center_y == null) this.center_y = this.canvas.height / 2;
            
            if(this.outer_radius == null) {
                // Need to set to half the width of the shortest dimension of the canvas as the canvas may not be square.
                // Minus the line segment line width otherwise the lines around the segments on the top,left,bottom,right
                // side are chopped by the edge of the canvas.
                if(this.canvas.width < this.canvas.height) {
                    this.outer_radius = (this.canvas.width / 2) - this.segment_outline_width;
                } else {
                    this.outer_radius = (this.canvas.height / 2) - this.segment_outline_width;
                }
            }
            
            // Also get a 2D context to the canvas as we need this to draw with.
            this.ctx = this.canvas.getContext("2d");
            
            this.offscreen_canvas = document.createElement('canvas');
            this.offscreen_canvas.width = this.canvas.width;
            this.offscreen_canvas.height = this.canvas.height;
            this.offscreen_ctx = this.offscreen_canvas.getContext("2d");
        }
    }
    
    this.segments = new Array();
    
    // If the text offset is null then set to same as font size as we want some by default.
    if(this.text_offset === null) {
        this.text_offset = (this.text_font_size / 1.7);
    }
    
    // If the animation options have been passed in then create animation object as a property of this class
    // and pass the options to it so the animation is set. Otherwise create default animation object.
    if((options != null) && (options.animation) && (typeof(options.animation) !== "undefined")) {
        this.animation = new Animation(options.animation);
    } else {
        this.animation = new Animation();
    }
    
    // If some pin options then create create a pin object and then pass them in.
    if((options != null) && (options.pins) && (typeof(options.pins) !== "undefined")) {
        this.pins = new Pin(options.pins);
    }
    
    // If the draw_mode is image change some defaults provided a value has not been specified.
    if((this.draw_mode == "image") || (this.draw_mode == "segment_image")) {
        // Remove grey segment_colour.
        if(typeof(options.segment_colour) === "undefined") {
            this.segment_colour = null;
        }
        
        // Set segment_outline_colour to red.
        if(typeof(options.segment_outline_colour) === "undefined") {
            this.segment_outline_colour = "red";
        }
        
        // Set draw_text to false as we will assume any text is part of the image.
        if(typeof(options.draw_text) === "undefined") {
            this.draw_text = false;
        }
        
        // Also set the segment_outline_width to 1 so that segment overlay will look correct.
        if(typeof(options.segment_outline_width) === "undefined") {
            this.segment_outline_width = 1;
        }
        
        // Set draw_wheel to false as normally the image needs to be loaded first.
        if(typeof(draw_wheel) === "undefined") {
            draw_wheel = false;
        }
    } else {
        // When in code draw_mode the default is the wheel will draw.
        if(typeof(draw_wheel) === "undefined") {
            draw_wheel = true;
        }
    }
    
    if((options != null) && (options.pointer_guide) && (typeof(options.pointer_guide) !== "undefined")) {
        this.pointer_guide = new Pointer_Guide(options.pointer_guide);
    } else {
        this.pointer_guide = new Pointer_Guide();
    }
    // Finally if draw_wheel is true then call function to render the wheel, segment text, overlay etc.
    if(draw_wheel == true) {
        if(this.clear_the_canvas) this.draw();
        else this.draw_no_clear();
    } else if(this.draw_mode == "segment_image") {
        // If segment image then loop though all the segments and load the images for them setting a callback
        // which will call the draw function of the wheel once all the images have been loaded.
        for(let i = 0; i < this.segments.length; i++) {
            if(this.segments[i].image !== null) {
                this.segments[i].img_data = this.make_segment_image(this.segments[i].image);
            }
        }
    }
}

Winwheel.prototype.make_segment_image = function(src) {
    function draw_wheel_when_all_images_are_loaded() {
        // Prevent multiple drawings of the wheel which ocurrs without this check due to timing of function calls.
        if(this.wheel_drawn_after_loading_all_images) return;
        
        let image_load_count = 0;
        let wheel = this.$wheel;
        // Loop though all the segments of the wheel and check if image data loaded, if so increment counter.
        for(let i = 0; i < wheel.segments.length; i++) {
            // Check the image data object is not null and also that the image has completed loading by checking
            // that a property of it such as the height has some sort of true value.
            if((wheel.segments[i].img_data != null) && (wheel.segments[i].img_data.height)) {
                image_load_count++;
            }
        }
        
        // If number of images loaded matches the segments then all the images for the wheel are loaded.
        if(image_load_count == wheel.segments.length) {
            this.wheel_drawn_after_loading_all_images = true;
            wheel.draw();
        }
    }
    
    this.wheel_drawn_after_loading_all_images = false;
    let image = new Image();
    image.$wheel = this; // So the wheel can be accessed in the callback
    image.onload = draw_wheel_when_all_images_are_loaded;
    image.src = src;
    return image;
};

// This function sorts out the segment sizes. Some segments may have set sizes, for the others what is left out of
// 360 degrees is shared evenly. What this function actually does is set the start and end angle of the arcs.
Winwheel.prototype.update_segment_angles_from_sizes = function() {
    if(this.segments) {
        let arc_used = 0;
        let num_set = 0;
        
        for(let i = 0; i < this.segments.length; i++) {
            if(this.segments[i].size !== null) {
                arc_used += this.segments[i].size;
                num_set++;
            }
        }
        
        let arc_left = (360 - arc_used);
        
        let degrees_each = 0;
        if(arc_left > 0) {
            degrees_each = (arc_left / (this.segments.length - num_set));
        }
        
        let current_degree = 0;
        for(let i = 0; i < this.segments.length; i++) {
            this.segments[i].start_angle = current_degree;
            if(this.segments[i].size) {
                current_degree += this.segments[i].size;
            } else {
                current_degree += degrees_each;
            }
            this.segments[i].end_angle = current_degree;
        }
    }
};

// This function clears the canvas. Will wipe anything else which happens to be drawn on it.
Winwheel.prototype.clear_canvas = function() {
    if(this.ctx) {
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }
};

Winwheel.prototype.redraw_whole_wheel = function() {
    if(!this.offscreen_ctx) return;
    let saved_rotation_angle = this.rotation_angle;
    this.rotation_angle = 0;
    this.offscreen_ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    
    // Call functions to draw the segments and then segment text.
    if(this.draw_mode == "image") {
        // Draw the wheel by loading and drawing an image such as a png on the canvas.
        this.draw_wheel_image();
        this.draw_segment_images();
        if(this.draw_segment_outlines_over_image == true) this.draw_segments();
        if(this.draw_text == true) this.draw_segment_text();
    } else {
        // The default operation is to draw the segments using code via the canvas arc() method.
        this.draw_segments();
        
        // The text is drawn on top.
        if(this.draw_text == true) this.draw_segment_text();
    }
    
    if(typeof this.pins !== "undefined") {
        if(this.pins.visible == true) this.draw_pins();
    }
    
    if(this.pointer_guide.display == true) this.draw_pointer_guide();
    
    this.rotation_angle = saved_rotation_angle;
};

Winwheel.prototype.draw_no_clear = function() {
    //this.redraw_whole_wheel();
    
    let ctx = this.ctx;
    let center_x = this.center_x;
    let center_y = this.center_y;
    
    // Rotate and then draw the wheel.
    // We must rotate by the rotation_angle before drawing to ensure that image wheels will spin.
    ctx.save();
    ctx.translate(center_x, center_y);
    ctx.rotate(this.deg_to_rad(this.rotation_angle));
    ctx.translate(-center_x, -center_y);
    
    // Draw the image passing the scaled width and height which will ensure the image will be responsive.
    this.ctx.drawImage(this.offscreen_canvas, 0, 0);
    
    ctx.restore();
};

// This function draws / re-draws the wheel on the canvas therefore rendering any changes.
Winwheel.prototype.draw = function() {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.draw_no_clear();
};

// Draws a line from the center of the wheel to the outside at the angle where the code thinks the pointer is.
Winwheel.prototype.draw_pointer_guide = function() {
    // If have canvas context.
    let ctx = this.offscreen_ctx;
    if(!ctx) return;
    let center_x = this.center_x;
    let center_y = this.center_y;
    let outer_radius = this.outer_radius;
    
    ctx.save();
    
    // Rotate the canvas to the line goes towards the location of the pointer.
    ctx.translate(center_x, center_y);
    ctx.rotate(this.deg_to_rad(this.pointer_angle));
    ctx.translate(-center_x, -center_y);
    
    // Set line colour and width.
    ctx.strokeStyle = this.pointer_guide.stroke_style;
    ctx.lineWidth   = this.pointer_guide.line_width;
    
    // Draw from the center of the wheel outwards past the wheel outer radius.
    ctx.beginPath();
    ctx.moveTo(center_x, center_y);
    ctx.lineTo(center_x, -(outer_radius / 4));
    
    ctx.stroke();
    ctx.restore();
};

// This function takes an image such as PNG and draws it on the canvas making its center at the center_x and center for the wheel.
Winwheel.prototype.draw_wheel_image = function() {
    // Double check the wheel_image property of this class is not null. This does not actually detect that an image
    // source was set and actually loaded so might get error if this is not the case. This is why the initial call
    // to draw() should be done from a wheel_image.onload callback as detailed in example documentation.
    if(this.wheel_image == null) return;
    let ctx = this.offscreen_ctx;
    let center_x = this.center_x;
    let center_y = this.center_y;
    
    let scaled_width  = this.image_scale*this.wheel_image.width;
    let scaled_height = this.image_scale*this.wheel_image.height;
    
    let dest_x = center_x - (scaled_width  / 2) + this.image_offset_x;
    let dest_y = center_y - (scaled_height / 2) - this.image_offset_y;
    
    // Rotate and then draw the wheel.
    // We must rotate by the rotation_angle before drawing to ensure that image wheels will spin.
    ctx.save();
    ctx.arc(center_x, center_y, this.outer_radius, 0, Math.PI * 2, false);
    ctx.clip();
    ctx.translate(center_x, center_y);
    ctx.rotate(this.deg_to_rad(this.rotation_angle + this.image_rotation));
    ctx.translate(-center_x, -center_y);
    
    // Draw the image passing the scaled width and height which will ensure the image will be responsive.
    ctx.drawImage(this.wheel_image, dest_x, dest_y, scaled_width, scaled_height);
    
    /*let image_data = ctx.getImageData(0, 0, scaled_width, scaled_height);
    
    for    (let y = 0; y < scaled_height; y++) {
        for(let x = 0; x < scaled_width;  x++) {
            let index = (y*4)*scaled_height + (x*4);
            let r = image_data.data[index];
            let g = image_data.data[index+1];
            let b = image_data.data[index+2];
            let average = (r+g+b)/3;
            image_data.data[index  ] = average*3;
            image_data.data[index+1] *= 0.2;
            image_data.data[index+2] *= 0.2;
        }
    }
    ctx.putImageData(image_data, 0, 0);*/
    
    ctx.restore();
};

// This function draws the wheel on the canvas by rendering the image for each segment.
Winwheel.prototype.draw_segment_images = function() {
    let ctx = this.offscreen_ctx;
    if(!ctx) return;
    if(!this.segments) return;
    
    let center_x = this.center_x;
    let center_y = this.center_y;
    
    for(let i = 0; i < this.segments.length; i++) {
        let seg = this.segments[i];
        if(!seg.img_data) continue;
        // Check image has loaded so a property such as height has a value.
        if(typeof seg.img_data.height !== "number" || !seg.img_data.height) {
            console.log("Segment " + i + " img_data is not loaded");
            continue;
        }
        // Work out the correct X and Y to draw the image at which depends on the direction of the image.
        // Images can be created in 4 directions. North, South, East, West.
        // North: Outside at top, inside at bottom. Sits evenly over the 0 degrees angle.
        // South: Outside at bottom, inside at top. Sits evenly over the 180 degrees angle.
        // East: Outside at right, inside at left. Sits evenly over the 90 degrees angle.
        // West: Outside at left, inside at right. Sits evenly over the 270 degrees angle.
        let image_left  = 0;
        let image_top   = 0;
        let image_angle = 0;
        let image_direction = "";
        let offset = (seg.image_offset) ? seg.image_offset : 0;
        
        // Get scaled width and height of the segment image.
        let scaled_width  = (seg.img_data.width );
        let scaled_height = (seg.img_data.height);
        
        image_direction = (seg.image_direction !== null) ? seg.image_direction : this.image_direction;
        
        if(image_direction == "S") {
            // Left set so image sits half/half over the 180 degrees point.
            image_left = (center_x - (scaled_width / 2));
            
            // Top so image starts at the center_y.
            image_top = center_y;
            
            // Angle to draw the image is its starting angle + half its size.
            // Here we add 180 to the angle to the segment is poistioned correctly.
            image_angle = (seg.start_angle + 180 + ((seg.end_angle - seg.start_angle) / 2));
        } else if(image_direction == "E") {
            // Left set so image starts and the center point.
            image_left = center_x;
            
            // Top is so that it sits half/half over the 90 degree point.
            image_top = (center_y - (scaled_height / 2));
            
            // Again get the angle in the center of the segment and add it to the rotation angle.
            // this time we need to add 270 to that to the segment is rendered the correct place.
            image_angle = (seg.start_angle + 270 + ((seg.end_angle - seg.start_angle) / 2));
        } else if(image_direction == "W") {
            // Left is the center_x minus the width of the image.
            image_left = (center_x - scaled_width);
            
            // Top is so that it sits half/half over the 270 degree point.
            image_top = (center_y - (scaled_height / 2));
            
            // Again get the angle in the center of the segment and add it to the rotation angle.
            // this time we need to add 90 to that to the segment is rendered the correct place.
            image_angle = (seg.start_angle + 90 + ((seg.end_angle - seg.start_angle) / 2));
        } else {
            // North is the default.
            // Left set so image sits half/half over the 0 degrees point.
            image_left = (center_x - (scaled_width / 2));
            
            // Top so image is its height out (above) the center point.
            image_top = (center_y - scaled_height);
            
            // Angle to draw the image is its starting angle + half its size.
            // this sits it half/half over the center angle of the segment.
            image_angle = (seg.start_angle + ((seg.end_angle - seg.start_angle) / 2));
        }
        
        ctx.save();
        ctx.translate(center_x, center_y);
        ctx.rotate(this.deg_to_rad(this.rotation_angle + image_angle));
        ctx.translate(-center_x, -center_y);
        
        ctx.drawImage(seg.img_data, image_left, image_top - offset, scaled_width, scaled_height);
        ctx.restore();
    }
};

// This function draws the wheel on the page by rendering the segments on the canvas.
Winwheel.prototype.draw_segments = function() {
    // Again check have context in case this function was called directly and not via draw function.
    let ctx = this.offscreen_ctx;
    if(!ctx) return;
    // Draw the segments if there is at least one in the segments array.
    if(!this.segments) return;
    // Get scaled center_x and center_y and also scaled inner and outer radius.
    let center_x     = this.center_x;
    let center_y     = this.center_y;
    let inner_radius = this.inner_radius;
    let outer_radius = this.outer_radius;
    
    let segrot_offset = this.segrot_offset;
    
    // Loop though and output all segments - position 0 of the array is not used, so start loop from index 1
    // this is to avoid confusion when talking about the first segment.
    for(let i = 0; i < this.segments.length; i++) {
        let seg = this.segments[i];
        
        let segment_colour = (seg.colour         !== null) ? seg.colour         : this.segment_colour;
        let outline_colour = (seg.outline_colour !== null) ? seg.outline_colour : this.segment_outline_colour;
        let outline_width  = (seg.outline_width  !== null) ? seg.outline_width  : this.segment_outline_width;
        
        if(!(segment_colour || outline_colour)) return;
        
        ctx.lineJoin = "round";
        ctx.fillStyle   = segment_colour;
        ctx.strokeStyle = outline_colour;
        ctx.lineWidth   = outline_width;
        
        // Begin a path as the segment consists of an arc and 2 lines.
        ctx.beginPath();
        
        // If don't have an inner radius then move to the center of the wheel as we want a line out from the center
        // to the start of the arc for the outside of the wheel when we arc. Canvas will draw the connecting line for us.
        if(!this.inner_radius) {
            ctx.moveTo(center_x, center_y);
        } else {
            // Work out the x and y values for the starting point of the segment which is at its starting angle
            // but out from the center point of the wheel by the value of the inner_radius. Some correction for line width is needed.
            let inner_x = Math.cos(this.deg_to_rad(seg.start_angle + this.rotation_angle - 90)) * (inner_radius - outline_width / 2);
            let inner_y = Math.sin(this.deg_to_rad(seg.start_angle + this.rotation_angle - 90)) * (inner_radius - outline_width / 2);
            
            // Now move here relative to the center point of the wheel.
            ctx.moveTo(center_x + inner_x, center_y + inner_y);
        }
        
        // Draw the outer arc of the segment clockwise in direction -->
        ctx.arc(center_x, center_y, outer_radius, this.deg_to_rad(seg.start_angle + this.rotation_angle - 90), this.deg_to_rad(seg.end_angle + this.rotation_angle - 90), false);
        
        if(this.inner_radius) {
            // Draw another arc, this time anticlockwise <-- at the inner_radius between the end angle and the start angle.
            // Canvas will draw a connecting line from the end of the outer arc to the beginning of the inner arc completing the shape.
            ctx.arc(center_x, center_y, inner_radius, this.deg_to_rad(seg.end_angle + this.rotation_angle - 90), this.deg_to_rad(seg.start_angle + this.rotation_angle - 90), true);
        } else {
            // If no inner radius then we draw a line back to the center of the wheel.
            ctx.lineTo(center_x, center_y);
        }
        
        if(segment_colour) ctx.fill();
        if(outline_colour) ctx.stroke();
    }
};

Winwheel.prototype.draw_segment_text = function() {
    let ctx = this.offscreen_ctx;
    if(!ctx) return;
    
    let center_x     = this.center_x;
    let center_y     = this.center_y;
    let outer_radius = this.outer_radius;
    let inner_radius = this.inner_radius;
    
    for(let seg_index = 0; seg_index < this.segments.length; seg_index++) {
        // Save the context so it is certain that each segment text option will not affect the other.
        ctx.save();
        
        let seg = this.segments[seg_index];
        if(!seg.text) continue;
        
        let font_family    = (seg.text_font_family    !== null) ? seg.text_font_family    : this.text_font_family;
        let font_size      = (seg.text_font_size      !== null) ? seg.text_font_size      : this.text_font_size;
        let font_weight    = (seg.text_font_weight    !== null) ? seg.text_font_weight    : this.text_font_weight;
        let orientation    = (seg.text_orientation    !== null) ? seg.text_orientation    : this.text_orientation;
        let alignment      = (seg.text_alignment      !== null) ? seg.text_alignment      : this.text_alignment;
        let offset         = (seg.text_offset         !== null) ? seg.text_offset         : this.text_offset;
        let text_colour    = (seg.text_colour         !== null) ? seg.text_colour         : this.text_colour;
        let outline_colour = (seg.text_outline_colour !== null) ? seg.text_outline_colour : this.text_outline_colour;
        let outline_width  = (seg.text_outline_width  !== null) ? seg.text_outline_width  : this.text_outline_width;
        
        // Scale the font size and the offset by the scale factor so the text can be responsive.
        font_size = (font_size);
        offset    = (offset   );
        
        // We need to put the font bits together in to one string.
        let font_setting = "";
        
        if(font_weight != null) font_setting += font_weight + " ";
        if(font_size   != null) font_setting += font_size + "px "; // Fonts on canvas are always a px value.
        if(font_family != null) font_setting += font_family;
        
        ctx.font        = font_setting;
        ctx.fillStyle   = text_colour;
        ctx.strokeStyle = outline_colour;
        ctx.lineWidth   = outline_width;
        
        if(orientation == "horizontal") {
            let lines = seg.text.split("\n");
            
            // Figure out the starting offset for the lines as when there are multiple lines need to center the text
            // vertically in the segment (when thinking of normal horizontal text).
            let line_offset = 0 - (font_size * (lines.length / 2)) + (font_size / 2);
            
            for(let line_index = 0; line_index < lines.length; line_index++) {
                let line = lines[line_index];
                if     (alignment == "inner") ctx.textAlign = "left";
                else if(alignment == "outer") ctx.textAlign = "right";
                else                          ctx.textAlign = "center";
                
                ctx.textBaseline = "middle";
                
                let text_angle = this.deg_to_rad(seg.end_angle - ((seg.end_angle - seg.start_angle) / 2) + this.rotation_angle - 90);
                
                ctx.save();
                ctx.translate(center_x, center_y);
                ctx.rotate(text_angle);
                ctx.translate(-center_x, -center_y);
                
                if(alignment == "inner") {
                    if(text_colour)      ctx.fillText(line, center_x + inner_radius + offset, center_y + line_offset);
                    if(outline_colour) ctx.strokeText(line, center_x + inner_radius + offset, center_y + line_offset);
                } else if(alignment == "outer") {
                    if(text_colour)      ctx.fillText(line, center_x + outer_radius - offset, center_y + line_offset);
                    if(outline_colour) ctx.strokeText(line, center_x + outer_radius - offset, center_y + line_offset);
                } else {
                    if(text_colour)      ctx.fillText(line, center_x + inner_radius + ((outer_radius - inner_radius) / 2) + offset, center_y + line_offset);
                    if(outline_colour) ctx.strokeText(line, center_x + inner_radius + ((outer_radius - inner_radius) / 2) + offset, center_y + line_offset);
                }
                ctx.restore();
                line_offset += font_size;
            }
        } else if(orientation == "vertical") {
            let lines = seg.text.split("\n");
            
            // Figure out the starting offset for the lines as when there are multiple lines need to center the text
            // vertically in the segment (when thinking of normal horizontal text).
            let line_offset = 0;// - (font_size * (lines.length / 2)) + (font_size / 2);
            let vertical_offset = offset;
            let text_angle = this.deg_to_rad(seg.end_angle - ((seg.end_angle - seg.start_angle) / 2) + this.rotation_angle);
            
            ctx.textAlign = "center";
            ctx.textBaseline = "top";
            
            for(let line_index = 0; line_index < lines.length; line_index++) {
                let line = lines[line_index];
                
                ctx.save();
                ctx.translate(center_x, center_y);
                ctx.rotate(text_angle);
                ctx.translate(-center_x, -center_y);
                
                if(text_colour) {
                    let center_of_donut = inner_radius + ((outer_radius - inner_radius) / 2);
                    ctx.fillText(line, center_x, center_y - (center_of_donut + vertical_offset - line_offset));
                }
                
                ctx.restore();
                line_offset += font_size;
            }
        }
        
        // Restore so all text options are reset ready for the next text.
        ctx.restore();
    }
    
};

// Converts degrees to radians which is what is used when specifying the angles on HTML5 canvas arcs.
Winwheel.prototype.deg_to_rad = function(d) {
    return d * 0.0174532925199432957;
};

// This function sets the center location of the wheel, saves a function call to set x then y.
Winwheel.prototype.set_center = function(x, y) {
    this.center_x = x;
    this.center_y = y;
};

// This function allows a segment to be added to the wheel. The position of the segment is optional,
// if not specified the new segment will be added to the end of the wheel.
Winwheel.prototype.add_segment = function(options, pos) {
    // Create a new segment object passing the options in.
    let new_segment = new Segment(options);
    
    // Work out where to place the segment, the default is simply as a new segment at the end of the wheel.
    if(typeof pos !== "undefined") {
        for(let i = this.segments.length; i > pos; i--) {
            this.segments[i] = this.segments[i - 1];
        }
    } else {
        pos = this.segments.length;
    }
    this.segments[pos] = new_segment;
    
    // Since a segment has been added the segment sizes need to be re-computed so call function to do this.
    this.update_segment_angles_from_sizes();
    
    // Return the segment object just created in the wheel (JavaScript will return it by reference), so that
    // further things can be done with it by the calling code if desired.
    return this.segments[pos];
};

// This function must be used if the canvas_id is changed as we also need to get the context of the new canvas.
Winwheel.prototype.set_canvas_id = function(canvas_id) {
    if(canvas_id) {
        this.canvas_id = canvas_id;
        this.canvas = document.getElementById(this.canvas_id);
        
        if(this.canvas) {
            this.ctx = this.canvas.getContext("2d");
        }
    } else {
        this.canvas_id = null;
        this.ctx = null;
        this.canvas = null;
    }
};

// This function deletes the specified segment from the wheel by removing it from the segments array.
Winwheel.prototype.delete_segment = function(pos) {
    // There needs to be at least one segment in order for the wheel to draw, so only allow delete if there
    // is more than one segment currently left in the wheel.
    
    //++ check that specifying a position that does not exist - say 10 in a 6 segment wheel does not cause issues.
    if(this.segments.length > 1) {
        // If the position of the segment to remove has been specified.
        if(typeof pos !== "undefined") {
            for(let i = pos + 1; i < this.segments.length; i++) {
                this.segments[i - 1] = this.segments[i];
            }
        }
        
        this.segments.length -= 1;
        this.update_segment_angles_from_sizes();
    }
};

// This function takes the x an the y of a mouse event, such as click or move, and converts the x and the y in to
// co-ordinates on the canvas as the raw values are the x and the y from the top and left of the user's browser.
Winwheel.prototype.window_to_canvas = function(x, y) {
    let bbox = this.canvas.getBoundingClientRect();
    return {
        x: Math.floor(x - bbox.left * (this.canvas.width  / bbox.width)),
        y: Math.floor(y - bbox.top  * (this.canvas.height / bbox.height))
    };
};

// This function returns the segment object located at the specified x and y coordinates on the canvas.
// It is used to allow things to be done with a segment clicked by the user, such as highlight, display or change some values, etc.
Winwheel.prototype.get_segment_at = function(x, y) {
    let segment_number = this.getSegmentNumberAt(x, y);
    if(segment_number !== null) {
        return this.segments[segment_number];
    }
    return null;
};

// Returns the number of the segment clicked instead of the segment object.
// This does not work correctly if the canvas width or height is altered by CSS but does work correctly with the scale factor.
Winwheel.prototype.getSegmentNumberAt = function(x, y) {
    // Call function above to convert the raw x and y from the user's browser to canvas coordinates
    // i.e. top and left is top and left of canvas, not top and left of the user's browser.
    let loc = this.window_to_canvas(x, y);
    
    // Now start the process of working out the segment clicked.
    // First we need to figure out the angle of an imaginary line between the center_x and center_y of the wheel and
    // the X and Y of the location (for example a mouse click).
    let top_bottom;
    let left_right;
    let adjacent_side_length;
    let opposite_side_length;
    let hypotenuse_side_length;
    
    // Get the center_x and center_y scaled with the scale factor, also the same for outer and inner radius.
    let center_x = (this.center_x);
    let center_y = (this.center_y);
    let outer_radius = (this.outer_radius);
    let inner_radius = (this.inner_radius);
    
    // We will use right triangle maths with the TAN function.
    // The start of the triangle is the wheel center, the adjacent side is along the x axis, and the opposite side is along the y axis.
    
    // We only ever use positive numbers to work out the triangle and the center of the wheel needs to be considered as 0 for the numbers
    // in the maths which is why there is the subtractions below. We also remember what quadrant of the wheel the location is in as we
    // need this information later to add 90, 180, 270 degrees to the angle worked out from the triangle to get the position around a 360 degree wheel.
    if(loc.x > center_x) {
        adjacent_side_length = (loc.x - center_x);
        left_right = "R"; // Location is in the right half of the wheel.
    } else {
        adjacent_side_length = (center_x - loc.x);
        left_right = "L"; // Location is in the left half of the wheel.
    }
    
    if(loc.y > center_y) {
        opposite_side_length = (loc.y - center_y);
        top_bottom = "B"; // Bottom half of wheel.
    } else {
        opposite_side_length = (center_y - loc.y);
        top_bottom = "T"; // Top Half of wheel.
    }
    
    // Now divide opposite by adjacent to get tan value.
    let tan_val = opposite_side_length / adjacent_side_length;
    
    // Use the tan function and convert results to degrees since that is what we work with.
    let result = (Math.atan(tan_val) * 180 / Math.PI);
    let location_angle = 0;
    
    // We also need the length of the hypotenuse as later on we need to compare this to the outer_radius of the segment / circle.
    hypotenuse_side_length = Math.sqrt((opposite_side_length * opposite_side_length) + (adjacent_side_length * adjacent_side_length));
    
    // Now to make sense around the wheel we need to alter the values based on if the location was in top or bottom half
    // and also right or left half of the wheel, by adding 90, 180, 270 etc. Also for some the initial location_angle needs to be inverted.
    if((top_bottom == "T") && (left_right == "R")) {
        location_angle = Math.round(90 - result);
    } else if((top_bottom == "B") && (left_right == "R")) {
        location_angle = Math.round(result + 90);
    } else if((top_bottom == "B") && (left_right == "L")) {
        location_angle = Math.round((90 - result) + 180);
    } else if((top_bottom == "T") && (left_right == "L")) {
        location_angle = Math.round(result + 270);
    }
    
    // And now we have to adjust to make sense when the wheel is rotated from the 0 degrees either
    // positive or negative and it can be many times past 360 degrees.
    if(this.rotation_angle != 0) {
        let rotated_position = this.get_rotation_position();
        
        // So we have this, now we need to alter the location_angle as a result of this.
        location_angle = (location_angle - rotated_position);
        
        // If negative then take the location away from 360.
        if(location_angle < 0) {
            location_angle = (360 - Math.abs(location_angle));
        }
    }
    
    // OK, so after all of that we have the angle of a line between the center_x and center_y of the wheel and
    // the X and Y of the location on the canvas where the mouse was clicked. Now time to work out the segment
    // this corresponds to. We can use the segment start and end angles for this.
    let found_segment_number = null;
    
    for(let i = 0; i < this.segments.length; i++) {
        // Due to segments sharing start and end angles, if line is clicked will pick earlier segment.
        if((location_angle >= this.segments[i].start_angle) && (location_angle <= this.segments[i].end_angle)) {
            // To ensure that a click anywhere on the canvas in the segment direction will not cause a
            // segment to be matched, as well as the angles, we need to ensure the click was within the radius
            // of the segment (or circle if no segment radius).
            
            // If the hypotenuse_side_length (length of location from the center of the wheel) is with the radius
            // then we can assign the segment to the found segment and break out the loop.
            
            // Have to take in to account hollow wheels (doughnuts) so check is greater than inner_radius as
            // well as less than or equal to the outer_radius of the wheel.
            if((hypotenuse_side_length >= inner_radius) && (hypotenuse_side_length <= outer_radius)) {
                found_segment_number = i;
                break;
            }
        }
    }
    
    // Finally return the number.
    return found_segment_number;
};

// Returns a reference to the segment that is at the location of the pointer on the wheel.
Winwheel.prototype.get_indicated_segment = function() {
    // Call function below to work this out and return the prize_number.
    let prize_number = this.get_indicated_segment_number();
    
    // Then simply return the segment in the segments array at that position.
    return this.segments[prize_number];
};

// Works out the segment currently pointed to by the pointer of the wheel. Normally called when the spinning has stopped
// to work out the prize the user has won. Returns the number of the segment in the segments array.
Winwheel.prototype.get_indicated_segment_number = function() {
    let indicated_prize = 0;
    let raw_angle = this.get_rotation_position();
    
    // Now we have the angle of the wheel, but we need to take in to account where the pointer is because
    // will not always be at the 12 o'clock 0 degrees location.
    let relative_angle = Math.floor(this.pointer_angle - raw_angle);
    
    if(relative_angle < 0) {
        relative_angle = 360 - Math.abs(relative_angle);
    }
    
    // Now we can work out the prize won by seeing what prize segment start_angle and end_angle the relative_angle is between.
    for(let i = 0; i < this.segments.length; i++) {
        if((relative_angle >= this.segments[i].start_angle) && (relative_angle <= this.segments[i].end_angle)) {
            indicated_prize = i;
            break;
        }
    }
    
    return indicated_prize;
};

// Works out what Pin around the wheel is considered the current one which is the one which just passed the pointer.
// Used to work out if the pin has changed during the animation to tigger a sound.
Winwheel.prototype.get_current_pin_number = function() {
    let current_pin = 0;
    
    if(this.pins) {
        let raw_angle = this.get_rotation_position();
        
        // Now we have the angle of the wheel, but we need to take in to account where the pointer is because
        // will not always be at the 12 o'clock 0 degrees location.
        let relative_angle = Math.floor(this.pointer_angle - raw_angle);
        
        if(relative_angle < 0) {
            relative_angle = 360 - Math.abs(relative_angle);
        }
        
        // Work out the angle of the pins as this is simply 360 / the number of pins as they space evenly around.
        let pin_spacing = (360 / this.pins.number);
        let total_pin_angle = 0;
        
        // Now we can work out the pin by seeing what pins relative_angle is between.
        for(let i = 0; i < (this.pins.number); i++) {
            if((relative_angle >= total_pin_angle) && (relative_angle <= (total_pin_angle + pin_spacing))) {
                current_pin = i;
                break;
            }
            
            total_pin_angle += pin_spacing;
        }
        
        // Now if rotating clockwise we must add 1 to the current pin as we want the pin which has just passed
        // the pointer to be returned as the current pin, not the start of the one we are between.
        if(this.animation.direction == "clockwise") {
            current_pin++;
            
            if(current_pin > this.pins.number) {
                current_pin = 0;
            }
        }
    }
    
    return current_pin;
};

// Returns the rotation angle of the wheel corrected to 0-360 (i.e. removes all the multiples of 360).
Winwheel.prototype.get_rotation_position = function() {
    let raw_angle = this.rotation_angle; // Get current rotation angle of wheel.
    
    // If positive work out how many times past 360 this is and then take the floor of this off the raw_angle.
    if(raw_angle >= 0) {
        if(raw_angle > 360) {
            // Get floor of the number of times past 360 degrees.
            let times_past_360 = Math.floor(raw_angle / 360);
            
            // Take all this extra off to get just the angle 0-360 degrees.
            raw_angle = (raw_angle - (360 * times_past_360));
        }
    } else {
        // Is negative, need to take off the extra then convert in to 0-360 degree value
        // so if, for example, was -90 then final value will be (360 - 90) = 270 degrees.
        if(raw_angle < -360) {
            let times_past_360 = Math.ceil(raw_angle / 360); // Ceil when negative.
            
            raw_angle = (raw_angle - (360 * times_past_360)); // Is minus because dealing with negative.
        }
        
        raw_angle = (360 + raw_angle); // Make in the range 0-360. Is plus because raw is still negative.
    }
    
    return raw_angle;
};

Winwheel.prototype.start_animation = function() {
    if(!this.animation) return;
    this.compute_animation();
    this.redraw_whole_wheel();
    
    let vars = {
        autoCSS       : false,
        ease          : this.animation.easing,
        callbackScope : this,
        onUpdate      : this.animation_loop, // Call function to re-draw the canvas.
        onComplete    : this.call_user_on_finished_callback, // Call function to perform actions when animation has finished.
    };
    vars[this.animation.property_name] = this.animation.property_value; // Here we set the property to be animated and its value.
    
    // Do the tween animation passing the vars from the animation object as an array of key => value pairs.
    // Keep reference to the tween object in the wheel as that allows pausing, resuming, and stopping while the animation is still running.
    this.tween = TweenLite.to(this, this.animation.duration, vars);
};

Winwheel.prototype.stop_animation = function(skip_user_callback) {
    if(this.tween) this.tween.kill();
    if(!skip_user_callback) this.call_user_on_finished_callback();
};

Winwheel.prototype.pause_animation = function() {
    if(this.tween) this.tween.pause();
};

Winwheel.prototype.resume_animation = function() {
    if(this.tween) this.tween.play();
};

// Called at the beginning of the start_animation function and computes the values needed to do the animation
// before it starts. This allows the developer to change the animation properties after the wheel has been created
// and have the animation use the new values of the animation properties.
Winwheel.prototype.compute_animation = function() {
    let anim = this.animation;
    if(!anim) return;
    // Set the animation parameters for the specified animation type including some sensible defaults if values have not been specified.
    if(anim.type == "spinOngoing") {
        // When spinning the rotation_angle is the wheel property which is animated.
        anim.property_name = "rotation_angle";
        
        if(anim.spins  == null) anim.spins  = 5;
        if(anim.easing == null) anim.easing = "Linear.easeNone";
        
        // We need to calculate the property_value and this is the spins * 360 degrees.
        anim.property_value = anim.spins * 360;
        
        // If the direction is anti-clockwise then make the property value negative.
        if(anim.direction == "anti-clockwise") {
            anim.property_value = 0 - anim.property_value;
        }
    } else if(anim.type == "spinToStop") {
        // Spin to stop the rotation angle is affected.
        anim.property_name = "rotation_angle";
        
        if(anim.spins  == null) anim.spins  = 5;
        if(anim.easing == null) anim.easing = "Power3.easeOut"; // This easing is fast start and slows over time.
        
        if(anim.stop_angle == null) {
            // If the stop angle has not been specified then pick random between 0 and 359.
            anim._stopAngle = Math.floor((Math.random() * 359));
        } else {
            // We need to set the internal to 360 minus what the user entered because the wheel spins past 0 without
            // this it would indicate the prize on the opposite side of the wheel. We aslo need to take in to account
            // the pointer_angle as the stop angle needs to be relative to that.
            anim._stopAngle = 360 - anim.stop_angle + this.pointer_angle;
        }
        
        // The property value is the spins * 360 then plus or minus the stop_angle depending on if the rotation is clockwise or anti-clockwise.
        anim.property_value = (anim.spins * 360);
        
        if(anim.direction == "anti-clockwise") {
            anim.property_value = (0 - anim.property_value);
            
            // Also if the value is anti-clockwise we need subtract the stop_angle (but to get the wheel to stop in the correct
            // place this is 360 minus the stop angle as the wheel is rotating backwards).
            anim.property_value -= (360 - anim._stopAngle);
        } else {
            // Add the stop_angle to the property_value as the wheel must rotate around to this place and stop there.
            anim.property_value += anim._stopAngle;
        }
    } else if(anim.type == "custom") {
        // Do nothing as all values must be set by the developer in the parameters
        // especially the property_name and property_value.
    }
};

// Calculates and returns a random stop angle inside the specified segment number. Value will always be 1 degree inside
// the start and end of the segment to avoid issue with the segment overlap.
Winwheel.prototype.get_random_for_segment = function(segment_number) {
    let stop_angle = 0;
    
    if(segment_number) {
        if(typeof this.segments[segment_number] !== "undefined") { // TODO: what if a segment is less than 2 degrees in size. put it in the middle
            let start_angle = this.segments[segment_number].start_angle;
            let end_angle = this.segments[segment_number].end_angle;
            let range = (end_angle - start_angle) - 2; // trim 1 degree off both sides for clarity
            
            if(range > 0) {
                stop_angle = (start_angle + 1 + Math.floor((Math.random() * range)));
            } else {
                console.log("Segment size is too small to safely get random angle inside it");
            }
        } else {
            console.log("Segment " + segment_number + " undefined");
        }
    } else {
        console.log("Segment number not specified");
    }
    
    return stop_angle;
};

// Class for the wheel pins.
function Pin(options) {
    let default_options = {
        visible: true, // In future there might be some functionality related to the pins even if they are not displayed.
        number: 36, // The number of pins. These are evenly distributed around the wheel.
        outer_radius: 3, // Radius of the pins which determines their size.
        fill_style: "grey", // Fill colour of the pins.
        stroke_style: "black", // Line colour of the pins.
        line_width: 1, // Line width of the pins.
        margin: 3, // The space between outside edge of the wheel and the pins.
    };
    
    // Now loop through the default options and create properties of this class set to the value for
    // the option passed in if a value was, or if not then set the value of the default.
    for(let key in default_options) {
        if((options != null) && (typeof(options[key]) !== "undefined")) {
            this[key] = options[key];
        } else {
            this[key] = default_options[key];
        }
    }
    
    // Also loop though the passed in options and add anything specified not part of the class in to it as a property.
    if(options != null) {
        for(let key in options) {
            if(typeof(this[key]) === "undefined") {
                this[key] = options[key];
            }
        }
    }
}

// Class for the wheel spinning animation which like a segment becomes a property of the wheel.
function Animation(options) {
    // Most of these options are null because the defaults are different depending on the type of animation.
    let default_options = {
        type: "spinOngoing", // For now there are only supported types are spinOngoing (continuous), spinToStop, custom.
        direction: "clockwise", // clockwise or anti-clockwise.
        property_name: null, // The name of the winning wheel property to be affected by the animation.
        property_value: null, // The value the property is to be set to at the end of the animation.
        duration: 10, // Duration of the animation.
        easing: null, // The easing to use for the animation, default is the best for spin to stop. Use Linear.easeNone for no easing.
        stop_angle: null, // Used for spinning, the angle at which the wheel is to stop.
        spins: null, // Used for spinning, the number of complete 360 degree rotations the wheel is to do.
        clear_the_canvas: null, // If set to true the canvas will be cleared before the wheel is re-drawn, false it will not, null the animation will abide by the value of this property for the parent wheel object.
        callback_finished: null, // Function to callback when the animation has finished.
        callback_before: null, // Function to callback before the wheel is drawn each animation loop.
        callback_after: null, // Function to callback after the wheel is drawn each animation loop.
        callback_sound: null, // Function to callback if a sound should be triggered on change of segment or pin.
        sound_trigger: "segment", // Sound trigger type. Default is segment which triggers when segment changes, can be pin if to trigger when pin passes the pointer.
    };
    
    // Now loop through the default options and create properties of this class set to the value for
    // the option passed in if a value was, or if not then set the value of the default.
    for(let key in default_options) {
        if((options != null) && (typeof(options[key]) !== "undefined")) {
            this[key] = options[key];
        } else {
            this[key] = default_options[key];
        }
    }
    
    // Also loop though the passed in options and add anything specified not part of the class in to it as a property.
    if(options != null) {
        for(let key in options) {
            if(typeof(this[key]) === "undefined") {
                this[key] = options[key];
            }
        }
    }
}

// Class for segments. When creating a json of options can be passed in.
function Segment(options) {
    // Define default options for segments, most are null so that the global defaults for the wheel
    // are used if the values for a particular segment are not specifically set.
    let default_options = {
        size: null, // Leave null for automatic. Valid values are degrees 0-360. Use percentToDegrees function if needed to convert.
        text: "", // Default is blank.
        colour: null, // If null for the rest the global default will be used.
        outline_colour: null,
        outline_width: null,
        text_font_family: null,
        text_font_size: null,
        text_font_weight: null,
        text_orientation: null,
        text_alignment: null,
        text_offset: null,
        text_colour: null,
        text_outline_colour: null,
        text_outline_width: null,
        image: null, // Name/path to the image
        image_direction: null, // Direction of the image, can be set globally for the whole wheel.
        img_data: null // Image object created here and loaded with image data.
    };
    
    // Now loop through the default options and create properties of this class set to the value for
    // the option passed in if a value was, or if not then set the value of the default.
    for(let key in default_options) {
        if((options != null) && (typeof(options[key]) !== "undefined")) {
            this[key] = options[key];
        } else {
            this[key] = default_options[key];
        }
    }
    
    // Also loop though the passed in options and add anything specified not part of the class in to it as a property.
    // This allows the developer to easily add properties to segments at construction time.
    if(options != null) {
        for(let key in options) {
            if(typeof(this[key]) === "undefined") {
                this[key] = options[key];
            }
        }
    }
    
    // There are 2 additional properties which are set by the code, so need to define them here.
    // They are not in the default options because they are not something that should be set by the user,
    // the values are updated every time the update_segment_angles_from_sizes() function is called.
    this.start_angle = 0;
    this.end_angle = 0;
}

// Changes an image for a segment by setting a callback to render the wheel once the image has loaded.
Segment.prototype.change_image = function(image, image_direction) {
    // Change image name, blank image data.
    this.image = image;
    this.img_data = null;
    
    if(image_direction) this.image_direction = image_direction;
    
    // Set img_data to a new image object, change set callback and change src (just like in wheel constructor).
    this.img_data = this.make_segment_image(this.image);
};

// Class that is created as property of the wheel. Draws line from center of the wheel out to edge of canvas to
// indicate where the code thinks the pointer location is. Helpful to get alignment correct esp when using images.
function Pointer_Guide(options) {
    let default_options = {
        display: false,
        stroke_style: "red",
        line_width: 3
    };
    
    // Now loop through the default options and create properties of this class set to the value for
    // the option passed in if a value was, or if not then set the value of the default.
    for(let key in default_options) {
        if((options != null) && (typeof(options[key]) !== "undefined")) {
            this[key] = options[key];
        } else {
            this[key] = default_options[key];
        }
    }
}

Winwheel.prototype.animation_loop = function() {
    // Check if the clear_the_canvas is specified for this animation, if not or it is not false then clear the canvas.
    if(this.animation.clear_the_canvas != false) {
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }
    
    if(typeof this.animation.callback_before === "function") this.animation.callback_before();
    this.draw_no_clear();
    if(typeof this.animation.callback_after  === "function") this.animation.callback_after();
    if(typeof this.animation.callback_sound  === "function") this.maybe_trigger_sound();
};

// This function figures out if the callback_sound function needs to be called by working out if the segment or pin
// has changed since the last animation loop.
Winwheel.prototype.maybe_trigger_sound = function() {
    if(!this.hasOwnProperty("_lastSoundTriggerNumber")) {
        this._lastSoundTriggerNumber = 0;
    }
    
    let callback_sound = this.animation.callback_sound;
    let current_trigger_number = 0;
    
    if(this.animation.sound_trigger == "pin") {
        current_trigger_number = this.get_current_pin_number();
    } else {
        current_trigger_number = this.get_indicated_segment_number();
    }
    
    if(current_trigger_number != this._lastSoundTriggerNumber) {
        callback_sound();
        this._lastSoundTriggerNumber = current_trigger_number;
    }
};

Winwheel.prototype.call_user_on_finished_callback = function() {
    if(typeof this.animation.callback_finished === "function") {
        this.animation.callback_finished();
    }
};

//~ END

// LICENSE:
//
// The MIT License (MIT)
//
// Copyright (c) 2012-2019 Douglas McKechie
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.