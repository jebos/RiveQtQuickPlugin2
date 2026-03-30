import argparse
import re
import subprocess
import sys
from pathlib import Path

SPIRV_FIXEDCOLOR_FRAG_INPUTS = [
    "atomic_draw_image_mesh.main",
    "atomic_draw_image_rect.main",
    "atomic_draw_interior_triangles.main",
    "atomic_draw_atlas_blit.main",
    "atomic_draw_path.main",
    "atomic_resolve.main",
    "draw_clockwise_path.main",
    "draw_clockwise_clip.main",
    "draw_clockwise_interior_triangles.main",
    "draw_clockwise_interior_triangles_clip.main",
    "draw_clockwise_atlas_blit.main",
    "draw_clockwise_image_mesh.main",
    "draw_clockwise_atomic_atlas_blit.main",
    "draw_clockwise_atomic_image_mesh.main",
    "draw_msaa_atlas_blit.main",
    "draw_msaa_image_mesh.main",
    "draw_msaa_path.main",
    "draw_msaa_stencil.main",
]

SPIRV_DRAW_MSAA_INPUTS = [
    "draw_msaa_path.main",
    "draw_msaa_image_mesh.main",
    "draw_msaa_atlas_blit.main",
]

SPIRV_ATOMIC_FRAG_OPT_PARAMS = [
    "--wrap-opkill",
    "--simplify-instructions",
    "--eliminate-dead-branches",
    "--merge-return",
    "--inline-entry-points-exhaustive",
    "--eliminate-dead-inserts",
    "--eliminate-dead-members",
    "--merge-blocks",
    "--redundancy-elimination",
    "--cfg-cleanup",
    "--eliminate-dead-const",
    "--eliminate-dead-variables",
    "--eliminate-dead-functions",
    "--eliminate-dead-code-aggressive",
]

SPIRV_STANDARD_FRAG_OPT_PARAMS = [
    "--wrap-opkill",
    "--eliminate-dead-branches",
    "--merge-return",
    "--inline-entry-points-exhaustive",
    "--eliminate-dead-functions",
    "--eliminate-dead-code-aggressive",
    "--private-to-local",
    "--eliminate-local-single-block",
    "--eliminate-local-single-store",
    "--eliminate-dead-code-aggressive",
    "--scalar-replacement=100",
    "--convert-local-access-chains",
    "--eliminate-local-single-block",
    "--eliminate-local-single-store",
    "--eliminate-dead-code-aggressive",
    "--ssa-rewrite",
    "--eliminate-dead-code-aggressive",
    "--ccp",
    "--eliminate-dead-code-aggressive",
    "--loop-unroll",
    "--eliminate-dead-branches",
    "--redundancy-elimination",
    "--combine-access-chains",
    "--scalar-replacement=100",
    "--convert-local-access-chains",
    "--eliminate-local-single-block",
    "--eliminate-local-single-store",
    "--eliminate-dead-code-aggressive",
    "--ssa-rewrite",
    "--eliminate-dead-code-aggressive",
    "--vector-dce",
    "--eliminate-dead-inserts",
    "--eliminate-dead-branches",
    "--if-conversion",
    "--copy-propagate-arrays",
    "--reduce-load-size",
    "--eliminate-dead-code-aggressive",
    "--merge-blocks",
    "--redundancy-elimination",
    "--eliminate-dead-branches",
    "--merge-blocks",
]


def run(command, cwd=None, env=None):
    subprocess.run(command, cwd=cwd, env=env, check=True)


def write_stamp(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("generated\n", encoding="utf-8")


def repair_glsl_token_boundaries(out_dir: Path) -> None:
    # The upstream minifier can collapse `)` directly into the next identifier
    # (for example `w1(c) v` -> `w1(c)v`), which Mesa rejects when compiling the
    # generated OpenGL shaders. Insert the separator back into the generated
    # artifacts we consume locally without patching the vendored runtime tree.
    generated_patterns = ("*.hpp", "*.minified.glsl", "*.minified.vert", "*.minified.frag")
    token_boundary_pattern = re.compile(r"\)(?=[A-Za-z_])")

    for pattern in generated_patterns:
        for path in out_dir.glob(pattern):
            text = path.read_text(encoding="utf-8")
            repaired = token_boundary_pattern.sub(") ", text)
            if repaired != text:
                path.write_text(repaired, encoding="utf-8")


def minify(source_dir: Path, out_dir: Path, ply_path: Path) -> None:
    inputs = []
    for pattern in ("*.glsl", "*.vert", "*.frag"):
        inputs.extend(sorted(source_dir.glob(pattern)))

    command = [
        sys.executable,
        str(source_dir / "minify.py"),
        "-o",
        str(out_dir),
        "-p",
        str(ply_path),
    ]
    command.extend(str(path) for path in inputs)
    run(command, cwd=source_dir)
    repair_glsl_token_boundaries(out_dir)


def compile_d3d(source_dir: Path, out_dir: Path, fxc: Path) -> None:
    d3d_dir = source_dir / "d3d"
    out_d3d = out_dir / "d3d"
    out_d3d.mkdir(parents=True, exist_ok=True)

    for shader in sorted(d3d_dir.glob("*.hlsl")):
        base = shader.stem
        run([
            str(fxc),
            "/D", "VERTEX",
            "/I", str(out_dir),
            "/T", "vs_5_0",
            "/Fh", str(out_d3d / f"{base}.vert.h"),
            str(shader),
        ])
        if base != "render_atlas":
            run([
                str(fxc),
                "/D", "FRAGMENT",
                "/I", str(out_dir),
                "/T", "ps_5_0",
                "/Fh", str(out_d3d / f"{base}.frag.h"),
                str(shader),
            ])

    render_atlas = d3d_dir / "render_atlas.hlsl"
    run([
        str(fxc),
        "/D", "FRAGMENT",
        "/D", "ATLAS_FEATHERED_STROKE",
        "/I", str(out_dir),
        "/T", "ps_5_0",
        "/Fh", str(out_d3d / "render_atlas_stroke.frag.h"),
        str(render_atlas),
    ])
    run([
        str(fxc),
        "/D", "FRAGMENT",
        "/D", "ATLAS_FEATHERED_FILL",
        "/I", str(out_dir),
        "/T", "ps_5_0",
        "/Fh", str(out_d3d / "render_atlas_fill.frag.h"),
        str(render_atlas),
    ])
    run([
        str(fxc),
        "/I", str(out_dir),
        "/T", "rootsig_1_1",
        "/E", "ROOT_SIG",
        "/Fh", str(out_d3d / "root.sig.h"),
        str(d3d_dir / "root.sig"),
    ])


def spirv_stage(output_type: str) -> str:
    return output_type.rsplit("_", 1)[-1]


def should_compile_spirv(input_path: Path, output_type: str) -> bool:
    stage = spirv_stage(output_type)
    if stage == "vert":
        return input_path.suffix != ".frag"
    return input_path.suffix != ".vert"


def spirv_output_basename(input_path: Path, output_type: str) -> str:
    return f"{input_path.stem}.{output_type}"


def spirv_symbol_name(input_path: Path, output_type: str) -> str:
    return input_path.name.replace(input_path.suffix, f"_{output_type}")


def is_atomic_input(input_path: Path) -> bool:
    return "atomic" in input_path.stem and "clockwise_atomic" not in input_path.stem


def is_clockwise_input(input_path: Path) -> bool:
    return "clockwise" in input_path.stem and "clockwise_atomic" not in input_path.stem


def spirv_compile_defines(input_path: Path, output_type: str) -> list[str]:
    defines = ["-DTARGET_VULKAN"]
    if spirv_stage(output_type) == "vert":
        defines.append("-DVERTEX")
    else:
        defines.append("-DFRAGMENT")
        if is_clockwise_input(input_path):
            defines.append("-DPLS_IMPL_STORAGE_TEXTURE")
        else:
            defines.append("-DPLS_IMPL_SUBPASS_LOAD")

    if output_type == "fixedcolor_frag":
        defines.append("-DFIXED_FUNCTION_COLOR_OUTPUT")
    elif output_type == "noclipdistance_vert":
        defines.append("-DDISABLE_CLIP_DISTANCE_FOR_UBERSHADERS")
    return defines


def spirv_opt_params(input_path: Path, output_type: str) -> list[str]:
    if spirv_stage(output_type) == "vert":
        return ["-O"]
    if is_atomic_input(input_path):
        return SPIRV_ATOMIC_FRAG_OPT_PARAMS
    return SPIRV_STANDARD_FRAG_OPT_PARAMS


def compile_spirv(source_dir: Path,
                  out_dir: Path,
                  glslang_validator: Path,
                  spirv_opt: Path) -> None:
    spirv_dir = source_dir / "spirv"
    out_spirv = out_dir / "spirv"
    out_spirv.mkdir(parents=True, exist_ok=True)

    standard_inputs = sorted(spirv_dir.glob("*.main"))
    standard_inputs += sorted(spirv_dir.glob("*.vert"))
    standard_inputs += sorted(spirv_dir.glob("*.frag"))

    def compile_variants(input_paths: list[Path], output_types: list[str]) -> None:
        for output_type in output_types:
            for input_path in input_paths:
                if not should_compile_spirv(input_path, output_type):
                    continue

                stage = spirv_stage(output_type)
                output_base = spirv_output_basename(input_path, output_type)
                spirv_output = out_spirv / f"{output_base}.spirv"
                unoptimized_output = out_spirv / f"{output_base}.spirv.unoptimized"
                header_output = out_spirv / f"{output_base}.h"

                compile_command = [
                    str(glslang_validator),
                    "-S",
                    stage,
                    *spirv_compile_defines(input_path, output_type),
                    f"-I{out_dir}",
                    "-V",
                    "-o",
                    str(unoptimized_output),
                    str(input_path),
                ]
                run(compile_command, cwd=source_dir)

                optimize_command = [
                    str(spirv_opt),
                    "--preserve-bindings",
                    "--preserve-interface",
                    *spirv_opt_params(input_path, output_type),
                    str(unoptimized_output),
                    "-o",
                    str(spirv_output),
                ]
                run(optimize_command, cwd=source_dir)

                run([
                    sys.executable,
                    str(source_dir / "spirv_binary_to_header.py"),
                    str(spirv_output),
                    str(header_output),
                    spirv_symbol_name(input_path, output_type),
                ], cwd=source_dir)

                unoptimized_output.unlink(missing_ok=True)

    compile_variants(standard_inputs, ["vert", "frag"])
    compile_variants(
        [spirv_dir / name for name in SPIRV_FIXEDCOLOR_FRAG_INPUTS],
        ["fixedcolor_frag"],
    )
    compile_variants(
        [spirv_dir / name for name in SPIRV_DRAW_MSAA_INPUTS],
        ["noclipdistance_vert"],
    )


def ensure_metal_toolchain() -> None:
    guidance = "Install it with: xcodebuild -downloadComponent MetalToolchain"
    try:
        subprocess.run(
            ["xcrun", "metal", "-help"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            ["xcrun", "-find", "metallib"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        raise RuntimeError(f"The Xcode Metal Toolchain is not available. {guidance}") from exc


def compile_metal(source_dir: Path,
                  out_dir: Path,
                  ply_path: Path,
                  targets: list[str]) -> None:
    if sys.platform != "darwin":
        raise RuntimeError("Metal shader generation is only supported on Darwin hosts.")

    ensure_metal_toolchain()

    target_map = {
        "macos": ("rive_pls_macosx_metallib", out_dir / "rive_pls_macosx.metallib.c"),
        "ios": ("rive_pls_ios_metallib", out_dir / "rive_pls_ios.metallib.c"),
        "ios-simulator": ("rive_pls_ios_simulator_metallib",
                           out_dir / "rive_pls_ios_simulator.metallib.c"),
    }
    selected_targets = targets or ["macos"]

    for target in selected_targets:
        make_target, required_output = target_map[target]
        run(
            [
                "make",
                "-C",
                str(source_dir),
                f"OUT={out_dir}",
                f"FLAGS=-p {ply_path}",
                make_target,
            ],
        )

        required_outputs = [
            out_dir / "draw_combinations.metal",
            required_output,
        ]
        missing_outputs = [path for path in required_outputs if not path.exists()]
        if missing_outputs:
            missing = ", ".join(str(path) for path in missing_outputs)
            raise RuntimeError(f"Metal shader generation did not produce: {missing}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--ply-path", required=True)
    parser.add_argument("--fxc")
    parser.add_argument("--glslang-validator")
    parser.add_argument("--spirv-opt")
    parser.add_argument("--metal", action="store_true")
    parser.add_argument(
        "--metal-target",
        action="append",
        choices=["macos", "ios", "ios-simulator"],
    )
    args = parser.parse_args()

    source_dir = Path(args.source_dir).resolve()
    out_dir = Path(args.out_dir).resolve()
    ply_path = Path(args.ply_path).resolve()

    out_dir.mkdir(parents=True, exist_ok=True)
    minify(source_dir, out_dir, ply_path)

    if args.fxc:
        compile_d3d(source_dir, out_dir, Path(args.fxc).resolve())
    if args.glslang_validator and args.spirv_opt:
        compile_spirv(source_dir,
                      out_dir,
                      Path(args.glslang_validator).resolve(),
                      Path(args.spirv_opt).resolve())
    if args.metal:
        compile_metal(source_dir, out_dir, ply_path, args.metal_target or [])

    write_stamp(out_dir / "stamp.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
