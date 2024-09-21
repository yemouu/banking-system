const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    { // Client build steps
        const exe = b.addExecutable(.{
            .name = "bank-client",
            .target = target,
            .optimize = optimize,
        });

        exe.linkLibC();
        exe.linkSystemLibrary("pthread");
        exe.addCSourceFiles(.{
            .root = b.path("src"),
            .files = &.{"bank-client.c"},
        });

        b.installArtifact(exe);

        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());

        if (b.args) |args| {
            run_cmd.addArgs(args);
        }

        const run_step = b.step(
            "run-client",
            "Run the client",
        );
        run_step.dependOn(&run_cmd.step);
    }

    { // Server build steps
        const exe = b.addExecutable(.{
            .name = "bank-server",
            .target = target,
            .optimize = optimize,
        });

        exe.linkLibC();
        exe.addCSourceFiles(.{
            .root = b.path("src"),
            .files = &.{"bank-server.c"},
        });

        b.installArtifact(exe);

        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());

        if (b.args) |args| {
            run_cmd.addArgs(args);
        }

        const run_step = b.step(
            "run-server",
            "Run the server",
        );
        run_step.dependOn(&run_cmd.step);
    }
}
