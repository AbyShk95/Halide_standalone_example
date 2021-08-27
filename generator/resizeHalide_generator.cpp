#include "Halide.h"

using namespace Halide;
class ResizeHalide : public Generator<ResizeHalide> {
public:
    Input<Buffer<uint8_t>> in{"in", 3};
    Input<float> sX{"sX"};
    Input<float> sY{"sY"};
    Output<Buffer<uint8_t>> out{"out", 3};

    void generate() {
        Expr x_coord = cast<int>(x * sX);
        Expr y_coord = cast<int>(y * sY);

        out(x,y,c) = in(x_coord, y_coord, c);

        out.dim(0).set_stride(4);
        out.dim(2).set_stride(1);

        in.dim(0).set_stride(4);
        in.dim(2).set_stride(1);

        in.dim(2).set_bounds(0, 4);
        out.dim(2).set_bounds(0, 4);

        in.dim(0).set_estimate(0, 1024);
        in.dim(1).set_estimate(0, 1024);
        in.dim(2).set_estimate(0, 4);

        sX.set_estimate(2.f);
        sY.set_estimate(2.f);

        out.dim(0).set_estimate(0, 512);
        out.dim(1).set_estimate(0, 512);
        out.dim(2).set_estimate(0,4);
    }

    void schedule() {
        if (!auto_schedule) {
            Var xi, yi;
            out
                .hexagon()
                //.tile(x, y, xi, yi, 128, 8)
                //.parallel(y)
                .vectorize(x, 128)
                ;
        }
    }

private:
    Var x{"x"}, y{"y"}, c{"c"};

};

HALIDE_REGISTER_GENERATOR(ResizeHalide, resizeHalide);
