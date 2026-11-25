import argparse
from datetime import datetime
import os
import sys
import numpy as np
import matplotlib

if "--no-show" in sys.argv:
    matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from hexss.pyconfig import Config
from sklearn import tree
from sklearn.decomposition import PCA
from sklearn.metrics import accuracy_score, confusion_matrix
from sklearn.model_selection import StratifiedKFold, cross_val_predict
from sklearn.preprocessing import StandardScaler

CLASSES = ["blue5", "white5", "white2", "mix"]
FEATURE_NAMES = ["S1_R", "S1_G", "S1_B", "S1_C", "S2_R", "S2_G", "S2_B", "S2_C"]

CLASS_COLORS = {
    "blue5": "#1f77b4",
    "white5": "#f39c12",  # สีทองอำพัน (มองเห็นชัดบนพื้นขาว)
    "white2": "#7f8c8d",  # สีเทาเข้ม (สื่อถึงความสว่างต่ำกว่า white5)
    "mix": "#9b59b6",
}

date = datetime.now().strftime('%y%m%d-%H%M%S')


def load_dataset():
    c = Config()
    X, y, active_classes = [], [], []
    for cls_name in CLASSES:
        samples = getattr(c, cls_name, None) or []
        if len(samples) > 0:
            active_classes.append(cls_name)
            for row in samples:
                if len(row) == 8:
                    X.append([float(v) for v in row])
                    y.append(cls_name)
    return np.array(X, dtype=float), np.array(y), active_classes


def print_summary_table(X, y, active_classes):
    print("\n================ Dataset Summary (Mean ± SD) ================")
    header = f"{'Class':<8} {'N':>4} | " + " | ".join(f"{f:>13}" for f in FEATURE_NAMES)
    print(header)
    print("-" * len(header))
    for cls in active_classes:
        sub = X[y == cls]
        stats = " | ".join(
            f"{sub[:, i].mean():6.1f}±{sub[:, i].std():5.1f}"
            for i in range(len(FEATURE_NAMES))
        )
        print(f"{cls:<8} {len(sub):>4} | {stats}")
    print("=" * len(header) + "\n")


def plot_fig1_distributions(X, y, active_classes, out_dir):
    """Figure 1: Boxplot + Mean สำหรับ Sensor 1 และ Sensor 2 (ครบทั้ง 8 ช่อง)"""
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.8), sharey=False)
    channels = ["R", "G", "B", "C"]
    n_cls = len(active_classes)
    group_width = 0.76
    box_w = group_width / max(n_cls, 1)

    sensor_configs = [
        (axes[0], "Sensor 1 (SDA:4, SCL:5) — Channel Distributions", 0),
        (axes[1], "Sensor 2 (SDA:6, SCL:7) — Channel Distributions", 4),
    ]

    for ax, title, offset_idx in sensor_configs:
        for c_i, cls in enumerate(active_classes):
            sub = X[y == cls]
            color = CLASS_COLORS.get(cls, "#333333")
            pos_shift = (c_i - (n_cls - 1) / 2.0) * box_w

            for ch_i in range(4):
                vals = sub[:, offset_idx + ch_i]
                pos = ch_i + pos_shift
                bp = ax.boxplot(
                    vals,
                    positions=[pos],
                    widths=box_w * 0.82,
                    patch_artist=True,
                    showmeans=True,
                    meanprops=dict(marker="D", markerfacecolor="white", markeredgecolor="black", markersize=5),
                    medianprops=dict(color="black", linewidth=1.4),
                    flierprops=dict(marker="o", markersize=4, markerfacecolor=color, alpha=0.6),
                )
                for patch in bp["boxes"]:
                    patch.set_facecolor(color)
                    patch.set_alpha(0.75)

        ax.set_title(title, fontsize=12, fontweight="bold")
        ax.set_xticks(range(4))
        ax.set_xticklabels(channels, fontsize=11)
        ax.set_xlabel("Channel")
        ax.set_ylabel("Raw Intensity")
        ax.grid(axis="y", linestyle="--", alpha=0.45)

        legend_handles = [
            Patch(facecolor=CLASS_COLORS.get(cls, "#333"), alpha=0.75, edgecolor="black", label=f"{cls} (N={np.sum(y == cls)})")
            for cls in active_classes
        ]
        ax.legend(handles=legend_handles, loc="upper left", fontsize=9)

    fig.suptitle("Figure 1: Raw Sensor Distributions (Box = IQR, Line = Median, Diamond = Mean)", fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    path = os.path.join(out_dir, f"{date} 01_sensor_distributions.png")
    fig.savefig(path, dpi=180)
    return path


def plot_fig2_cluster_physics_cm(X, y, active_classes, clf_params, out_dir):
    """Figure 2: 2D PCA Cluster + Confusion Matrix (CV) + Physical Scatter (S1 & S2)"""
    fig, axes = plt.subplots(2, 2, figsize=(14.5, 11))
    ax_pca, ax_cm = axes[0, 0], axes[0, 1]
    ax_s1, ax_s2 = axes[1, 0], axes[1, 1]

    # --- 2.1 PCA 2D Cluster Scatter ---
    if X.shape[0] >= 2:
        X_scaled = StandardScaler().fit_transform(X)
        pca = PCA(n_components=2, random_state=42)
        X_pca = pca.fit_transform(X_scaled)
        var = pca.explained_variance_ratio_ * 100

        for cls in active_classes:
            mask = (y == cls)
            color = CLASS_COLORS.get(cls, "#333")
            ax_pca.scatter(
                X_pca[mask, 0], X_pca[mask, 1],
                c=color, label=f"{cls} (N={np.sum(mask)})",
                alpha=0.75, edgecolors="k", linewidths=0.5, s=55
            )
            # พล็อตจุดศูนย์กลางกลุ่ม (Centroid)
            cx, cy = X_pca[mask, 0].mean(), X_pca[mask, 1].mean()
            ax_pca.scatter(cx, cy, c=color, marker="X", s=160, edgecolors="black", linewidths=1.2, zorder=6)

        ax_pca.set_title("2D Cluster Separation (PCA on 8 Features)", fontsize=11, fontweight="bold")
        ax_pca.set_xlabel(f"PC1 ({var[0]:.1f}% variance)")
        ax_pca.set_ylabel(f"PC2 ({var[1]:.1f}% variance)")
        ax_pca.grid(True, linestyle="--", alpha=0.4)
        ax_pca.legend(fontsize=9)
    else:
        ax_pca.text(0.5, 0.5, "Need at least 2 samples for PCA", ha="center", va="center")

    # --- 2.2 Confusion Matrix (Stratified Cross-Validation) ---
    class_counts = [int(np.sum(y == cls)) for cls in active_classes]
    min_count = min(class_counts) if class_counts else 0
    n_splits = min(5, min_count)

    if len(active_classes) >= 2 and n_splits >= 2:
        skf = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)
        cv_clf = tree.DecisionTreeClassifier(**clf_params)
        y_pred = cross_val_predict(cv_clf, X, y, cv=skf)
        acc = accuracy_score(y, y_pred) * 100
        cm_title = f"Confusion Matrix ({n_splits}-Fold CV | Acc: {acc:.1f}%)"
    else:
        # กรณีข้อมูลบางคลาสยังมีแค่ 1 ตัวอย่าง ให้แสดง Train CM พร้อมแจ้งเตือน
        tmp_clf = tree.DecisionTreeClassifier(**clf_params).fit(X, y)
        y_pred = tmp_clf.predict(X)
        acc = accuracy_score(y, y_pred) * 100
        cm_title = f"Confusion Matrix (Train Only — need >=2/class for CV | Acc: {acc:.1f}%)"

    cm = confusion_matrix(y, y_pred, labels=active_classes)
    im = ax_cm.imshow(cm, interpolation="nearest", cmap="Blues")
    fig.colorbar(im, ax=ax_cm, fraction=0.046, pad=0.04)
    ax_cm.set_title(cm_title, fontsize=11, fontweight="bold")
    ax_cm.set_xticks(range(len(active_classes)))
    ax_cm.set_yticks(range(len(active_classes)))
    ax_cm.set_xticklabels(active_classes)
    ax_cm.set_yticklabels(active_classes)
    ax_cm.set_xlabel("Predicted Label")
    ax_cm.set_ylabel("True Label")

    thresh = cm.max() / 2.0 if cm.max() > 0 else 0
    row_sums = cm.sum(axis=1, keepdims=True)
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            pct = (cm[i, j] / row_sums[i, 0] * 100) if row_sums[i, 0] > 0 else 0
            ax_cm.text(
                j, i, f"{cm[i, j]}\n({pct:.0f}%)",
                ha="center", va="center",
                color="white" if cm[i, j] > thresh else "black",
                fontsize=9, fontweight="bold"
            )

    # --- 2.3 & 2.4 Physical Scatter: Blue Ratio vs Brightness (C) ---
    for ax, title, idx in [
        (ax_s1, "Physical Space — Sensor 1 (Blue Ratio vs Brightness)", 0),
        (ax_s2, "Physical Space — Sensor 2 (Blue Ratio vs Brightness)", 4),
    ]:
        for cls in active_classes:
            sub = X[y == cls]
            rgb_sum = np.maximum(sub[:, idx] + sub[:, idx + 1] + sub[:, idx + 2], 1.0)
            blue_ratio = sub[:, idx + 2] / rgb_sum
            clear_val = sub[:, idx + 3]
            ax.scatter(
                blue_ratio, clear_val,
                c=CLASS_COLORS.get(cls, "#333"),
                label=cls, alpha=0.8, edgecolors="k", linewidths=0.5, s=55
            )
        ax.set_title(title, fontsize=11, fontweight="bold")
        ax.set_xlabel("Blue Chromaticity Ratio: B / (R + G + B)")
        ax.set_ylabel("Clear Brightness (Raw C)")
        ax.grid(True, linestyle="--", alpha=0.4)
        ax.legend(fontsize=9)

    fig.suptitle("Figure 2: Cluster Separation, Cross-Validation Accuracy & Physical Space", fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    path = os.path.join(out_dir, f"{date} 02_cluster_physics_cm.png")
    fig.savefig(path, dpi=180)
    return path


def plot_fig3_explainability(X, y, clf_params, out_dir):
    """Figure 3: Decision Tree Diagram + Feature Importance"""
    clf = tree.DecisionTreeClassifier(**clf_params)
    clf.fit(X, y)

    fig, (ax_tree, ax_imp) = plt.subplots(
        1, 2, figsize=(16, 6.8), gridspec_kw={"width_ratios": [2.4, 1]}
    )

    # --- 3.1 Decision Tree Diagram ---
    tree.plot_tree(
        clf,
        feature_names=FEATURE_NAMES,
        class_names=list(clf.classes_),
        filled=True,
        rounded=True,
        fontsize=9,
        ax=ax_tree,
    )
    ax_tree.set_title("Decision Tree Rules (Thresholds per Node)", fontsize=12, fontweight="bold")

    # --- 3.2 Feature Importance ---
    importances = clf.feature_importances_ * 100.0
    order = np.argsort(importances)
    sorted_names = [FEATURE_NAMES[i] for i in order]
    sorted_vals = importances[order]
    bar_colors = ["#2e86c1" if n.startswith("S1") else "#28b463" for n in sorted_names]

    bars = ax_imp.barh(sorted_names, sorted_vals, color=bar_colors, edgecolor="black", alpha=0.85)
    ax_imp.set_title("Feature Importance (%)", fontsize=12, fontweight="bold")
    ax_imp.set_xlabel("Importance (%)")
    ax_imp.set_xlim(0, max(sorted_vals.max() * 1.22, 10))
    ax_imp.grid(axis="x", linestyle="--", alpha=0.45)

    for bar, val in zip(bars, sorted_vals):
        ax_imp.text(val + 0.8, bar.get_y() + bar.get_height() / 2, f"{val:.1f}%", va="center", fontsize=9)

    ax_imp.legend(
        handles=[
            Patch(facecolor="#2e86c1", edgecolor="black", label="Sensor 1"),
            Patch(facecolor="#28b463", edgecolor="black", label="Sensor 2"),
        ],
        loc="lower right",
        fontsize=9,
    )

    fig.suptitle("Figure 3: Decision Tree Model Explainability", fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    path = os.path.join(out_dir, f"{date} 03_model_explainability.png")
    fig.savefig(path, dpi=180)
    return path


def generate_report(out_dir="reports", show=True):
    X, y, active_classes = load_dataset()
    if len(X) == 0:
        print("[ERROR] No data found in Config(). Collect samples in main.py first.")
        return

    os.makedirs(out_dir, exist_ok=True)
    print_summary_table(X, y, active_classes)

    min_count = min(int(np.sum(y == cls)) for cls in active_classes)
    clf_params = {
        "max_depth": 4,
        "min_samples_leaf": 2 if min_count >= 4 else 1,
        "random_state": 42,
    }

    p1 = plot_fig1_distributions(X, y, active_classes, out_dir)
    p2 = plot_fig2_cluster_physics_cm(X, y, active_classes, clf_params, out_dir)
    p3 = plot_fig3_explainability(X, y, clf_params, out_dir)

    print("Saved charts:")
    print(f"  1) {p1}")
    print(f"  2) {p2}")
    print(f"  3) {p3}")

    if show:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate TCS34725 classification summary charts")
    parser.add_argument("--out", default="reports", help="Output directory for PNG charts")
    parser.add_argument("--no-show", action="store_true", help="Save PNGs without opening plot windows")
    args = parser.parse_args()

    generate_report(out_dir=args.out, show=not args.no_show)