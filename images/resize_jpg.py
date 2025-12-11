#!/usr/bin/env python3
"""
resize_png.py

Pad an image equally on both axes to make it square, then resize to a target size and save as png.
Uses block-based downsampling: each block is reduced to its most common color.

Usage:
  python resize_png.py input.png output.png --size 64
"""
import argparse
import os
from pathlib import Path
from PIL import Image
from collections import Counter
import numpy as np
from sklearn.cluster import KMeans


def block_based_resize(img, size=64):
    """
    Resize image using block-based downsampling:
    - Divide padded image into block_size x block_size blocks.
    - Each block is reduced to a single pixel with the most common color.
    """
    w, h = img.size
    size_block_x = w // size
    size_block_y = h // size

    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out_px = out.load()
    img_px = img.load()

    for by in range(size):
        for bx in range(size):
            # Define block boundaries
            x_start = bx * size_block_x
            y_start = by * size_block_y
            x_end = min((bx + 1) * size_block_x, w)
            y_end = min((by + 1) * size_block_y, h)

            # Count colors in this block
            color_count = Counter()
            for y in range(y_start, y_end):
                for x in range(x_start, x_end):
                    rgba = img_px[x, y]
                    # Include transparent pixels but separately
                    color_count[rgba] += 1

            # Get most common color
            if color_count:
                most_common_color, _ = color_count.most_common(1)[0]
                out_px[bx, by] = most_common_color

    return out


def trim_transparent(img):
    """
    Remove all rows and columns that contain only transparent pixels.
    """
    w, h = img.size
    img_px = img.load()

    # Find leftmost non-transparent column
    left = w
    for x in range(w):
        for y in range(h):
            if img_px[x, y][3] >= 128:  # alpha >= 128 is non-transparent
                left = min(left, x)
                break

    # Find rightmost non-transparent column
    right = -1
    for x in range(w - 1, -1, -1):
        for y in range(h):
            if img_px[x, y][3] >= 128:
                right = max(right, x)
                break

    # Find topmost non-transparent row
    top = h
    for y in range(h):
        for x in range(w):
            if img_px[x, y][3] >= 128:
                top = min(top, y)
                break

    # Find bottommost non-transparent row
    bottom = -1
    for y in range(h - 1, -1, -1):
        for x in range(w):
            if img_px[x, y][3] >= 128:
                bottom = max(bottom, y)
                break

    # Crop to bounding box
    if left <= right and top <= bottom:
        trimmed = img.crop((left, top, right + 1, bottom + 1))
        tw, th = trimmed.size
        print(f"Trimmed transparent pixels: {w}x{h} -> {tw}x{th}")
        return trimmed
    else:
        print("Image is entirely transparent; no trimming")
        return img


def cluster_colors(img, n_colors):
    """
    Cluster image colors to N distinct colors using k-means.
    All pixels in the same cluster are remapped to the centroid color.
    If the image already has fewer than N unique colors, use the actual count.
    """
    w, h = img.size
    img_array = np.array(img)  # Shape: (h, w, 4) for RGBA
    
    # Reshape to list of pixels
    pixels = img_array.reshape(-1, 4).astype(np.float32)
    
    # Count unique colors in the image
    unique_colors = len(np.unique(pixels, axis=0))
    print(f"Image has {unique_colors} unique colors")
    
    # If image has fewer colors than requested, use actual count
    actual_n_colors = min(n_colors, unique_colors)
    print(f"Clustering to {actual_n_colors} colors")
    
    if actual_n_colors == unique_colors:
        # No clustering needed
        return img
    
    # K-means clustering
    kmeans = KMeans(n_clusters=actual_n_colors, n_init=10, random_state=42)
    labels = kmeans.fit_predict(pixels)
    
    # Get cluster centroids and convert to uint8
    centroids = kmeans.cluster_centers_.astype(np.uint8)
    
    # Remap pixels to centroids
    new_pixels = centroids[labels]
    
    # Reshape back to image
    new_array = new_pixels.reshape(h, w, 4)
    new_img = Image.fromarray(new_array, 'RGBA')
    
    print(f"Color clustering complete: remapped to {actual_n_colors} colors")
    return new_img

def majority_filter(img, kernel=3, iterations=1):
    """
    Apply a majority (mode) filter on the image labels.
    This replaces each pixel with the most common color in its kxk neighborhood.
    Implemented on integer labels for speed.
    """
    if kernel <= 1 or iterations <= 0:
        return img

    arr = np.array(img)
    h, w = arr.shape[0], arr.shape[1]
    pixels = arr.reshape(-1, 4)

    # Map unique RGBA colors to integer labels
    colors, inv = np.unique(pixels, axis=0, return_inverse=True)
    labels = inv.reshape(h, w)

    pad = kernel // 2
    for _ in range(iterations):
        new_labels = labels.copy()
        for y in range(h):
            y0 = max(0, y - pad)
            y1 = min(h, y + pad + 1)
            for x in range(w):
                x0 = max(0, x - pad)
                x1 = min(w, x + pad + 1)
                neigh = labels[y0:y1, x0:x1].ravel()
                # bincount for small neighborhoods is fine
                counts = np.bincount(neigh)
                new_labels[y, x] = np.argmax(counts)
        labels = new_labels

    new_pixels = colors[labels.ravel()].reshape(h, w, 4).astype(np.uint8)
    return Image.fromarray(new_pixels, 'RGBA')


def pad_to_square_and_resize(in_path, size=64, n_colors=None):
    img = Image.open('images/input_img/'+in_path).convert('RGBA')
    w, h = img.size
    
    # Trim transparent pixels first
    img = trim_transparent(img)
    w, h = img.size
    
    # Apply color clustering if requested
    if n_colors is not None:
        img = cluster_colors(img, n_colors)
        w, h = img.size

    # Pad each dimension independently so the final padded width and height
    # are the next multiple of `size`. This does not force a square; it makes
    # width = ceil(w/size)*size and height = ceil(h/size)*size.
    new_w = w if w % size == 0 else ((w + size - 1) // size) * size
    new_h = h if h % size == 0 else ((h + size - 1) // size) * size
    max_d = max(new_w, new_h)
    new_w = max_d
    new_h = max_d
    
    print(new_w, new_h)
    if (w, h) != (new_w, new_h):
        pad = Image.new('RGBA', (new_w, new_h), (0, 0, 0, 0))
        ox = (new_w - w) // 2
        oy = (new_h - h) // 2
        pad.paste(img, (ox, oy), img if img.mode == 'RGBA' else None)
        img = pad
        print(f"Padded to {new_w}x{new_h} (multiples of {size}), offset ({ox},{oy})")

    # Resize using block-based method or standard PIL resize
    print(f"Resizing using block-based method: {new_w}x{new_h} -> {size}x{size} ({new_w//size}x{new_h//size} blocks)")
    img = block_based_resize(img, size)


    img.save('images/output_img/'+in_path, 'png')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Pad an image, optionally cluster colors, and resize using block-based method')
    parser.add_argument('input')
    parser.add_argument('--size', type=int, default=64, help='Target output size (default: 64)')
    parser.add_argument('--colors', type=int, default=None, help='Number of distinct colors to cluster to (default: no clustering)')
    args = parser.parse_args()

    pad_to_square_and_resize(
        args.input,
        size=args.size,
        n_colors=args.colors,
    )

